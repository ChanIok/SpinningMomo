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
    app.get("/api/screenshots", [&screenshot_service](auto* res, auto* req) {
        try {
            int64_t last_id = 0;
            int limit = 20;
            
            if (auto last_id_param = Request::GetQueryParam(req, "lastId")) {
                last_id = std::stoll(*last_id_param);
            }
            if (auto limit_param = Request::GetQueryParam(req, "limit")) {
                limit = std::stoi(*limit_param);
            }
            
            // 检查是否有年月参数
            auto year_param = Request::GetQueryParam(req, "year");
            auto month_param = Request::GetQueryParam(req, "month");
            
            std::pair<std::vector<Screenshot>, bool> result;
            
            if (year_param && month_param) {
                // 如果有年月参数，使用按月份查询
                int year = std::stoi(*year_param);
                int month = std::stoi(*month_param);
                result = screenshot_service.get_screenshots_by_month(year, month, last_id, limit);
            } else {
                // 否则使用普通分页查询
                result = screenshot_service.get_screenshots_paginated(last_id, limit);
            }
            
            auto [screenshots, has_more] = result;
            
            nlohmann::json response = {
                {"screenshots", nlohmann::json::array()},
                {"hasMore", has_more}
            };
            
            for (const auto& screenshot : screenshots) {
                response["screenshots"].push_back(
                    screenshot_service.get_screenshot_with_thumbnail(screenshot));
            }
            
            Response::Success(res, response);
        } catch (const std::exception& e) {
            spdlog::error("Error getting screenshots: {}", e.what());
            Response::Error(res, "Internal server error", 500);
        }
    });

    // 获取单张截图
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

    // 获取截图原始图片内容
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

    // 获取相册中的所有截图
    app.get("/api/albums/:album_id/screenshots", [&screenshot_service, &SCREENSHOT_DIR](auto* res, auto* req) {
        try {
            auto screenshots = screenshot_service.get_screenshots_by_directory(SCREENSHOT_DIR.wstring());
            
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& screenshot : screenshots) {
                json_array.push_back(screenshot_service.get_screenshot_with_thumbnail(screenshot));
            }
            Response::Success(res, json_array);
        } catch (const std::exception& e) {
            spdlog::error("Error getting album screenshots: {}", e.what());
            Response::Error(res, "Album not found", 404);
        }
    });

    // 更新截图信息
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
                Response::Error(res, e.what());
            }
        });
    });

    // 删除截图
    app.del("/api/screenshots/:id", [&screenshot_service](auto* res, auto* req) {
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

    // 获取月份统计信息
    app.get("/api/screenshots/calendar", [&screenshot_service](auto* res, auto* req) {
        try {
            auto stats = screenshot_service.get_month_statistics();
            Response::Success(res, stats);
        } catch (const std::exception& e) {
            spdlog::error("Error getting month statistics: {}", e.what());
            Response::Error(res, "Failed to get month statistics", 500);
        }
    });

    // 获取缩略图
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
            Response::Error(res, "Internal server error", 500);
        }
    });
} 