#pragma once

#include <httplib.h>
#include <spdlog/spdlog.h>
#include "media/services/service_manager.hpp"
#include "media/utils/logger.hpp"
#include <filesystem>

// 注册截图相关路由
inline void register_screenshot_routes(httplib::Server& server) {
    auto& screenshot_service = ScreenshotService::get_instance();
    
    // 使用filesystem::path来处理路径
    const std::filesystem::path SCREENSHOT_DIR = LR"(D:\Program Files\InfinityNikki Launcher\InfinityNikki\X6Game\ScreenShot)";

    // 获取截图列表（支持分页）
    server.Get("/api/screenshots", [screenshot_dir = SCREENSHOT_DIR.wstring()](const httplib::Request& req, httplib::Response& res) {
        try {
            // 获取查询参数
            int64_t last_id = 0;
            int limit = 20;
            if (req.has_param("lastId")) {
                last_id = std::stoll(req.get_param_value("lastId"));
            }
            if (req.has_param("limit")) {
                limit = std::stoi(req.get_param_value("limit"));
            }
            
            // 获取截图列表
            auto screenshots = Screenshot::find_by_directory(screenshot_dir, last_id, limit);
            bool has_more = Screenshot::has_more(screenshot_dir, screenshots.empty() ? 0 : screenshots.back().id);
            
            // 构造响应
            nlohmann::json response = {
                {"screenshots", nlohmann::json::array()},
                {"hasMore", has_more}
            };
            
            for (const auto& screenshot : screenshots) {
                response["screenshots"].push_back(screenshot.to_json());
            }
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            spdlog::error("Error getting screenshots: {}", e.what());
            res.status = 500;
            res.set_content(R"({"error":"Internal server error"})", "application/json");
        }
    });

    // 获取单张截图
    server.Get(R"(/api/screenshots/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto id = std::stoll(req.matches[1]);
            
            // 查找指定ID的截图
            auto screenshots = Screenshot::find_by_directory(SCREENSHOT_DIR.wstring());
            auto it = std::find_if(screenshots.begin(), screenshots.end(),
                [id](const Screenshot& s) { return s.id == id; });
            
            if (it != screenshots.end()) {
                res.set_content(it->to_json().dump(), "application/json");
            } else {
                res.status = 404;
                res.set_content(R"({"error":"Screenshot not found"})", "application/json");
            }
        } catch (const std::exception& e) {
            spdlog::error("Error getting screenshot: {}", e.what());
            res.status = 500;
            res.set_content(R"({"error":"Internal server error"})", "application/json");
        }
    });

    // 获取截图原始图片内容
    server.Get(R"(/api/screenshots/(\d+)/raw)", [screenshot_dir = SCREENSHOT_DIR.wstring()](const httplib::Request& req, httplib::Response& res) {
        try {
            auto id = std::stoll(req.matches[1]);
            
            // 查找指定ID的截图
            auto screenshots = Screenshot::find_by_directory(screenshot_dir);
            auto it = std::find_if(screenshots.begin(), screenshots.end(),
                [id](const Screenshot& s) { return s.id == id; });
            
            if (it == screenshots.end()) {
                res.status = 404;
                res.set_content(R"({"error":"Screenshot not found"})", "application/json");
                return;
            }
            
            // 读取图片文件
            std::ifstream file(it->filepath, std::ios::binary);
            if (!file) {
                res.status = 404;
                res.set_content(R"({"error":"Image file not found"})", "application/json");
                return;
            }

            // 获取文件扩展名并设置对应的Content-Type
            auto ext = std::filesystem::path(it->filepath).extension().string();
            std::string content_type;
            if (ext == ".jpg" || ext == ".jpeg") {
                content_type = "image/jpeg";
            } else if (ext == ".png") {
                content_type = "image/png";
            } else {
                content_type = "application/octet-stream";
            }

            // 读取文件内容
            std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
            
            // 设置响应头和内容
            res.set_header("Content-Type", content_type);
            res.set_header("Cache-Control", "public, max-age=31536000");
            res.body.assign(buffer.begin(), buffer.end());

        } catch (const std::exception& e) {
            spdlog::error("Error serving raw image: {}", e.what());
            res.status = 500;
            res.set_content(R"({"error":"Internal server error"})", "application/json");
        }
    });

    // 获取相册中的所有截图
    server.Get(R"(/api/albums/(\d+)/screenshots)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto album_id = std::stoll(req.matches[1]);
            
            auto screenshots = Screenshot::find_by_directory(SCREENSHOT_DIR.wstring());
            
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& screenshot : screenshots) {
                json_array.push_back(screenshot.to_json());
            }
            res.set_content(json_array.dump(), "application/json");
        } catch (const std::exception& e) {
            spdlog::error("Error getting album screenshots: {}", e.what());
            res.status = 404;
            res.set_content(R"({"error":"Album not found"})", "application/json");
        }
    });

    // 更新截图信息
    server.Put(R"(/api/screenshots/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto id = std::stoll(req.matches[1]);
            
            auto json = nlohmann::json::parse(req.body);
            auto screenshot = screenshot_service.get_screenshot(id);
            
            // 只允许更新某些字段
            if (json.contains("metadata")) {
                screenshot.metadata = json["metadata"].get<std::string>();
            }
            
            if (screenshot_service.update_screenshot(screenshot)) {
                res.set_content(screenshot.to_json().dump(), "application/json");
            } else {
                res.status = 404;
                res.set_content(R"({"error":"Screenshot not found"})", "application/json");
            }
        } catch (const std::exception& e) {
            spdlog::error("Error updating screenshot: {}", e.what());
            res.status = 400;
            res.set_content(R"({"error":")" + std::string(e.what()) + R"("})", "application/json");
        }
    });

    // 删除截图
    server.Delete(R"(/api/screenshots/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        auto id = std::stoll(req.matches[1]);
        
        if (screenshot_service.delete_screenshot(id)) {
            res.status = 204;
        } else {
            res.status = 404;
            res.set_content(R"({"error":"Screenshot not found"})", "application/json");
        }
    });
} 