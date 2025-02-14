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

    // GET /api/screenshots - 获取截图列表
    app.get("/api/screenshots", [&screenshot_service](auto* res, auto* req) {
        try {
            int64_t last_id = 0;
            int limit = 20;
            std::string folder_id;
            std::string relative_path;
            
            if (auto last_id_param = Request::GetQueryParam(req, "lastId")) {
                last_id = std::stoll(*last_id_param);
            }
            if (auto limit_param = Request::GetQueryParam(req, "limit")) {
                limit = std::stoi(*limit_param);
            }
            if (auto folder_param = Request::GetQueryParam(req, "folderId")) {
                folder_id = *folder_param;
            }
            if (auto path_param = Request::GetQueryParam(req, "relativePath")) {
                relative_path = *path_param;
            }
            
            auto year_param = Request::GetQueryParam(req, "year");
            auto month_param = Request::GetQueryParam(req, "month");
            
            std::pair<std::vector<Screenshot>, bool> result;
            
            if (year_param && month_param) {
                int year = std::stoi(*year_param);
                int month = std::stoi(*month_param);
                result = screenshot_service.get_screenshots_by_month(year, month, last_id, limit);
            } else {
                result = screenshot_service.get_screenshots_paginated(folder_id, relative_path, last_id, limit);
            }
            
            auto [screenshots, has_more] = result;
            
            nlohmann::json response = {
                {"items", nlohmann::json::array()},
                {"hasMore", has_more}
            };
            
            for (const auto& screenshot : screenshots) {
                response["items"].push_back(
                    screenshot_service.get_screenshot_with_thumbnail(screenshot));
            }
            
            Response::Success(res, response);
        } catch (const std::exception& e) {
            spdlog::error("Error getting screenshots: {}", e.what());
            Response::Error(res, "Failed to get screenshots", 500);
        }
    });

    // GET /api/screenshots/:id - 获取单张截图
    app.get("/api/screenshots/:id", [&screenshot_service](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            auto screenshot = screenshot_service.get_screenshot(id);
            Response::Success(res, screenshot_service.get_screenshot_with_thumbnail(screenshot));
        } catch (const std::exception& e) {
            spdlog::error("Error getting screenshot: {}", e.what());
            Response::Error(res, "Screenshot not found", 404);
        }
    });

    // GET /api/screenshots/:id/raw - 获取截图原始图片内容
    app.get("/api/screenshots/:id/raw", [&screenshot_service](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            auto screenshot = screenshot_service.get_screenshot(id);
            
            auto [buffer, content_type] = screenshot_service.read_raw_image(screenshot);
            
            res->writeHeader("Content-Type", content_type);
            res->writeHeader("Cache-Control", "public, max-age=31536000");
            res->end(std::string_view(buffer.data(), buffer.size()));
            
        } catch (const std::exception& e) {
            spdlog::error("Error serving raw image: {}", e.what());
            Response::Error(res, "Internal server error", 500);
        }
    });

    // GET /api/albums/:album_id/screenshots - 获取相册中的所有截图
    app.get("/api/albums/:album_id/screenshots", [&screenshot_service](auto* res, auto* req) {
        try {
            int64_t last_id = 0;
            int limit = 20;
            auto album_id = std::stoll(Request::GetPathParam(req, 0));
            
            if (auto last_id_param = Request::GetQueryParam(req, "lastId")) {
                last_id = std::stoll(*last_id_param);
            }
            if (auto limit_param = Request::GetQueryParam(req, "limit")) {
                limit = std::stoi(*limit_param);
            }
            
            auto [screenshots, has_more] = screenshot_service.get_screenshots_by_album(album_id, last_id, limit);
            
            nlohmann::json response = {
                {"items", nlohmann::json::array()},
                {"hasMore", has_more}
            };
            
            for (const auto& screenshot : screenshots) {
                response["items"].push_back(screenshot_service.get_screenshot_with_thumbnail(screenshot));
            }
            
            Response::Success(res, response);
        } catch (const std::exception& e) {
            spdlog::error("Error getting album screenshots: {}", e.what());
            Response::Error(res, "Album not found", 404);
        }
    });

    // PUT /api/screenshots/:id - 更新截图信息
    app.put("/api/screenshots/:id", [&screenshot_service](auto* res, auto* req) {
        auto id = std::stoll(Request::GetPathParam(req, 0));
        
        res->onData([res, id, &screenshot_service](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto screenshot = screenshot_service.get_screenshot(id);
                
                if (json.contains("metadata")) {
                    screenshot.metadata = json["metadata"].get<std::string>();
                }
                
                if (screenshot_service.update_screenshot(screenshot)) {
                    Response::Success(res, screenshot_service.get_screenshot_with_thumbnail(screenshot));
                } else {
                    Response::Error(res, "Screenshot not found", 404);
                }
            } catch (const std::exception& e) {
                spdlog::error("Error updating screenshot: {}", e.what());
                Response::Error(res, "Failed to update screenshot");
            }
        });
    });

    // DELETE /api/screenshots/:id - 删除截图
    app.del("/api/screenshots/:id", [&screenshot_service](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            if (screenshot_service.delete_screenshot(id)) {
                Response::SuccessMessage(res, "Screenshot deleted successfully");
            } else {
                Response::Error(res, "Screenshot not found", 404);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to delete screenshot: {}", e.what());
            Response::Error(res, "Failed to delete screenshot");
        }
    });

    // GET /api/screenshots/calendar - 获取月份统计信息
    app.get("/api/screenshots/calendar", [&screenshot_service](auto* res, auto* req) {
        try {
            auto stats = screenshot_service.get_month_statistics();
            Response::Success(res, stats);
        } catch (const std::exception& e) {
            spdlog::error("Error getting month statistics: {}", e.what());
            Response::Error(res, "Failed to get month statistics", 500);
        }
    });

    // GET /api/screenshots/:id/thumbnail - 获取缩略图
    app.get("/api/screenshots/:id/thumbnail", [&screenshot_service, &thumbnail_service](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            auto screenshot = screenshot_service.get_screenshot(id);
            auto thumbnail_path = thumbnail_service.get_thumbnail_path(screenshot);
            
            if (!std::filesystem::exists(thumbnail_path) && 
                !thumbnail_service.generate_thumbnail(screenshot)) {
                Response::Error(res, "Thumbnail not found", 404);
                return;
            }
            
            std::ifstream file(thumbnail_path, std::ios::binary);
            if (!file) {
                spdlog::error("Failed to open thumbnail file: {}", thumbnail_path.string());
                Response::Error(res, "Failed to read thumbnail", 500);
                return;
            }
            
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            std::vector<char> buffer(size);
            if (!file.read(buffer.data(), size)) {
                spdlog::error("Failed to read thumbnail data");
                Response::Error(res, "Failed to read thumbnail", 500);
                return;
            }
            
            res->writeHeader("Content-Type", "image/webp");
            res->writeHeader("Cache-Control", "public, max-age=31536000");
            res->end(std::string_view(buffer.data(), buffer.size()));
            
        } catch (const std::exception& e) {
            spdlog::error("Error serving thumbnail: {}", e.what());
            Response::Error(res, "Failed to get thumbnail", 500);
        }
    });

    // GET /api/folders/tree - 获取文件夹树结构
    app.get("/api/folders/tree", [&screenshot_service](auto* res, auto* req) {
        try {
            auto tree = screenshot_service.get_folder_tree();
            Response::Success(res, tree);
        } catch (const std::exception& e) {
            spdlog::error("Error getting folder tree: {}", e.what());
            Response::Error(res, "Failed to get folder tree", 500);
        }
    });
} 