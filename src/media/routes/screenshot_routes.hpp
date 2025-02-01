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
#include "../services/thumbnail_service.hpp"
#include "../services/initialization_service.hpp"
#include "../db/models.hpp"
#include "../db/database.hpp"

// 注册截图相关路由
inline void register_screenshot_routes(uWS::App& app) {
    auto& screenshot_service = ScreenshotService::get_instance();
    auto& thumbnail_service = ThumbnailService::get_instance();
    auto& init_service = InitializationService::get_instance();
    
    // 获取截图目录路径
    const auto& SCREENSHOT_DIR = init_service.get_screenshot_directory();

    // 获取截图列表（支持分页）
    app.get("/api/screenshots", [](auto* res, auto* req) {
        try {
            // 获取查询参数
            int64_t last_id = 0;
            int limit = 20;
            
            if (auto last_id_param = Request::GetQueryParam(req, "lastId")) {
                last_id = std::stoll(*last_id_param);
            }
            if (auto limit_param = Request::GetQueryParam(req, "limit")) {
                limit = std::stoi(*limit_param);
            }
            
            // 从数据库获取截图列表
            auto screenshots = Screenshot::find_all(false);
            
            // 应用分页
            auto start = screenshots.begin();
            if (last_id > 0) {
                start = std::find_if(screenshots.begin(), screenshots.end(),
                    [last_id](const Screenshot& s) { return s.id > last_id; });
            }
            
            auto end = start;
            std::advance(end, std::min<int>(limit, std::distance(start, screenshots.end())));
            
            bool has_more = end != screenshots.end();
            
            // 构造响应
            nlohmann::json response = {
                {"screenshots", nlohmann::json::array()},
                {"hasMore", has_more}
            };
            
            // 添加缩略图信息
            for (auto it = start; it != end; ++it) {
                auto screenshot_json = it->to_json();
                
                // 添加缩略图URL
                if (ThumbnailService::get_instance().thumbnail_exists(*it)) {
                    screenshot_json["thumbnailPath"] = "/api/screenshots/" + 
                        std::to_string(it->id) + "/thumbnail";
                }
                
                response["screenshots"].push_back(screenshot_json);
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

    // 添加获取缩略图的路由
    app.get("/api/screenshots/:id/thumbnail", [&](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            spdlog::info("Thumbnail requested for screenshot ID: {}", id);
            
            // 尝试获取截图信息
            Screenshot screenshot;
            try {
                screenshot = screenshot_service.get_screenshot(id);
                spdlog::info("Found screenshot: {} ({})", screenshot.filename, screenshot.id);
            } catch (const std::runtime_error& e) {
                spdlog::error("Failed to find screenshot {}: {}", id, e.what());
                Response::Error(res, "Screenshot not found", 404);
                return;
            }
            
            auto thumbnail_path = thumbnail_service.get_thumbnail_path(screenshot);
            spdlog::info("Looking for thumbnail at: {}", thumbnail_path.string());
            
            if (!std::filesystem::exists(thumbnail_path)) {
                spdlog::error("Thumbnail file does not exist: {}", thumbnail_path.string());
                // 尝试重新生成缩略图
                spdlog::info("Attempting to regenerate thumbnail...");
                if (thumbnail_service.generate_thumbnail(screenshot)) {
                    spdlog::info("Successfully regenerated thumbnail");
                } else {
                    spdlog::error("Failed to regenerate thumbnail");
                    Response::Error(res, "Thumbnail not found", 404);
                    return;
                }
            }
            
            // 读取缩略图文件
            std::ifstream file(thumbnail_path, std::ios::binary);
            if (!file) {
                spdlog::error("Failed to open thumbnail file: {}", thumbnail_path.string());
                Response::Error(res, "Failed to read thumbnail", 500);
                return;
            }
            
            // 获取文件大小
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            spdlog::info("Reading thumbnail file, size: {} bytes", size);
            
            std::vector<char> buffer(size);
            if (!file.read(buffer.data(), size)) {
                spdlog::error("Failed to read thumbnail data");
                Response::Error(res, "Failed to read thumbnail", 500);
                return;
            }
            
            // 设置响应头和内容
            res->writeHeader("Content-Type", "image/webp");
            res->writeHeader("Cache-Control", "public, max-age=31536000");
            res->end(std::string_view(buffer.data(), buffer.size()));
            spdlog::info("Successfully sent thumbnail response");
            
        } catch (const std::exception& e) {
            spdlog::error("Error serving thumbnail: {}", e.what());
            Response::Error(res, "Internal server error", 500);
        }
    });
} 