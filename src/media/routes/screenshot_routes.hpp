#pragma once

#include <uwebsockets/App.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include "media/utils/logger.hpp"
#include "media/utils/response.hpp"
#include "media/utils/request.hpp"
#include "../services/service_manager.hpp"
#include "../services/screenshot_service.hpp"
#include "../db/models.hpp"

// 注册截图相关路由
inline void register_screenshot_routes(uWS::App& app) {
    auto& screenshot_service = ScreenshotService::get_instance();
    
    // 使用filesystem::path来处理路径
    const std::filesystem::path SCREENSHOT_DIR = LR"(D:\Program Files\InfinityNikki Launcher\InfinityNikki\X6Game\ScreenShot)";

    // 获取截图列表（支持分页）
    app.get("/api/screenshots", [screenshot_dir = SCREENSHOT_DIR.wstring()](auto* res, auto* req) {
        try {
            OutputDebugStringA("Get screenshots\n");
            // 获取查询参数
            int64_t last_id = 0;
            int limit = 20;
            
            if (auto last_id_param = Request::GetQueryParam(req, "lastId")) {
                last_id = std::stoll(*last_id_param);
            }
            if (auto limit_param = Request::GetQueryParam(req, "limit")) {
                limit = std::stoi(*limit_param);
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
            
            Response::Success(res, response);
        } catch (const std::exception& e) {
            spdlog::error("Error getting screenshots: {}", e.what());
            Response::Error(res, "Internal server error", 500);
        }
    });

    // 获取单张截图
    app.get("/api/screenshots/:id", [&](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            
            // 查找指定ID的截图
            auto screenshots = Screenshot::find_by_directory(SCREENSHOT_DIR.wstring());
            auto it = std::find_if(screenshots.begin(), screenshots.end(),
                [id](const Screenshot& s) { return s.id == id; });
            
            if (it != screenshots.end()) {
                Response::Success(res, it->to_json());
            } else {
                Response::Error(res, "Screenshot not found", 404);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error getting screenshot: {}", e.what());
            Response::Error(res, "Internal server error", 500);
        }
    });

    // 获取截图原始图片内容
    app.get("/api/screenshots/:id/raw", [screenshot_dir = SCREENSHOT_DIR.wstring()](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            
            // 查找指定ID的截图
            auto screenshots = Screenshot::find_by_directory(screenshot_dir);
            auto it = std::find_if(screenshots.begin(), screenshots.end(),
                [id](const Screenshot& s) { return s.id == id; });
            
            if (it == screenshots.end()) {
                Response::Error(res, "Screenshot not found", 404);
                return;
            }
            
            // 读取图片文件
            std::ifstream file(it->filepath, std::ios::binary);
            if (!file) {
                Response::Error(res, "Image file not found", 404);
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
            res->writeHeader("Content-Type", content_type);
            res->writeHeader("Cache-Control", "public, max-age=31536000");
            res->end(std::string_view(buffer.data(), buffer.size()));

        } catch (const std::exception& e) {
            spdlog::error("Error serving raw image: {}", e.what());
            Response::Error(res, "Internal server error", 500);
        }
    });

    // 获取相册中的所有截图
    app.get("/api/albums/:album_id/screenshots", [&](auto* res, auto* req) {
        try {
            auto album_id = std::stoll(Request::GetPathParam(req, 0));
            
            auto screenshots = Screenshot::find_by_directory(SCREENSHOT_DIR.wstring());
            
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& screenshot : screenshots) {
                json_array.push_back(screenshot.to_json());
            }
            Response::Success(res, json_array.dump());
        } catch (const std::exception& e) {
            spdlog::error("Error getting album screenshots: {}", e.what());
            Response::Error(res, "Album not found", 404);
        }
    });

    // 更新截图信息
    app.put("/api/screenshots/:id", [&](auto* res, auto* req) {
        auto id = std::stoll(Request::GetPathParam(req, 0));
        
        res->onData([res, id, &screenshot_service](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto screenshot = screenshot_service.get_screenshot(id);
                
                // 只允许更新某些字段
                if (json.contains("metadata")) {
                    screenshot.metadata = json["metadata"].get<std::string>();
                }
                
                if (screenshot_service.update_screenshot(screenshot)) {
                    Response::Success(res, screenshot.to_json());
                } else {
                    Response::Error(res, "Screenshot not found", 404);
                }
            } catch (const std::exception& e) {
                spdlog::error("Error updating screenshot: {}", e.what());
                Response::Error(res, e.what());
            }
        });
    });

    // 删除截图
    app.del("/api/screenshots/:id", [&](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            if (screenshot_service.delete_screenshot(id)) {
                Response::NoContent(res);
            } else {
                Response::Error(res, "Screenshot not found", 404);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to delete screenshot: {}", e.what());
            Response::Error(res, e.what());
        }
    });
} 