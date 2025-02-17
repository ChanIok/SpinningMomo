#pragma once

#include <uwebsockets/App.h>
#include <spdlog/spdlog.h>
#include "media/utils/logger.hpp"
#include "media/utils/response.hpp"
#include "media/utils/request.hpp"
#include "media/services/album/album_service.hpp"
#include "media/services/screenshot/screenshot_service.hpp"

/**
 * @brief 注册相册相关的所有路由
 * @param app uWebSockets应用实例
 */
inline void register_album_routes(uWS::App& app) {
    auto& album_service = AlbumService::get_instance();

    // GET /api/albums - 获取所有相册列表
    app.get("/api/albums", [&](auto* res, auto* req) {
        try {
            auto albums = album_service.get_albums();
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& album : albums) {
                json_array.push_back(album);
            }
            Response::Success(res, json_array);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get albums: {}", e.what());
            Response::Error(res, "Failed to get albums", 500);
        }
    });

    // POST /api/albums - 创建新相册
    app.post("/api/albums", [&](auto* res, auto* req) {
        struct Context {
            uWS::HttpResponse<false>* response;
            AlbumService& service;
        };
        
        auto ctx = std::make_shared<Context>(Context{res, album_service});
        
        res->onAborted([ctx]() {
            spdlog::warn("Request aborted while creating album");
        });
        
        res->onData([ctx](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto album = ctx->service.create_album(
                    json["name"].get<std::string>(),
                    json.value("description", "")
                );
                Response::Success(ctx->response, album);
            } catch (const std::exception& e) {
                spdlog::error("Failed to create album: {}", e.what());
                Response::Error(ctx->response, "Failed to create album");
            }
        });
    });

    // GET /api/albums/:id - 获取单个相册详情
    app.get("/api/albums/:id", [&](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            auto album = album_service.get_album(id);
            Response::Success(res, album);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get album: {}", e.what());
            Response::Error(res, "Album not found", 404);
        }
    });

    // PUT /api/albums/:id - 更新相册信息
    app.put("/api/albums/:id", [&](auto* res, auto* req) {
        auto id = std::stoll(Request::GetPathParam(req, 0));
        
        res->onData([res, id, &album_service](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto album = album_service.get_album(id);
                album.name = json["name"].get<std::string>();
                album.description = json.value("description", album.description);
                
                if (album_service.update_album(album)) {
                    Response::Success(res, album);
                } else {
                    Response::Error(res, "Album not found", 404);
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to update album: {}", e.what());
                Response::Error(res, "Failed to update album");
            }
        });
    });

    // DELETE /api/albums/:id - 删除相册
    app.del("/api/albums/:id", [&](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            if (album_service.delete_album(id)) {
                Response::SuccessMessage(res, "Album deleted successfully");
            } else {
                Response::Error(res, "Album not found", 404);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to delete album: {}", e.what());
            Response::Error(res, "Failed to delete album");
        }
    });

    // POST /api/albums/:id/screenshots - 添加截图到相册
    app.post("/api/albums/:id/screenshots", [&](auto* res, auto* req) {
        struct Context {
            uWS::HttpResponse<false>* response;
            AlbumService& service;
            int64_t album_id;
        };
        
        try {
            auto album_id = std::stoll(Request::GetPathParam(req, 0));
            auto ctx = std::make_shared<Context>(Context{res, album_service, album_id});
            
            res->onAborted([ctx]() {
                spdlog::warn("Request aborted while adding screenshots to album");
            });
            
            res->onData([ctx](std::string_view body, bool last) {
                if (!last) return;
                
                try {
                    auto json = Request::ParseJson(body);
                    auto screenshot_ids = json["screenshot_ids"].get<std::vector<int64_t>>();
                    
                    if (ctx->service.add_screenshots_to_album(ctx->album_id, screenshot_ids)) {
                        Response::SuccessMessage(ctx->response, "Screenshots added to album successfully");
                    } else {
                        Response::Error(ctx->response, "Failed to add screenshots to album");
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Failed to add screenshots to album: {}", e.what());
                    Response::Error(ctx->response, "Failed to add screenshots to album");
                }
            });
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse album id: {}", e.what());
            Response::Error(res, "Invalid album ID");
        }
    });

    // GET /api/albums/:id/screenshots - 获取相册中的截图
    app.get("/api/albums/:id/screenshots", [&](auto* res, auto* req) {
        try {
            auto album_id = std::stoll(Request::GetPathParam(req, 0));
            auto last_id = Request::GetQueryParam(req, "lastId");
            auto limit = Request::GetQueryParam(req, "limit");

            auto [screenshots, has_more] = ScreenshotService::get_instance().get_screenshots_by_album(
                album_id,
                !last_id.has_value() ? 0 : std::stoll(*last_id),
                std::stoi(limit.value_or("20"))
            );

            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& screenshot : screenshots) {
                json_array.push_back(
                    ScreenshotService::get_instance().get_screenshot_with_thumbnail(screenshot));
            }
            Response::Success(res, json_array);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get album screenshots: {}", e.what());
            Response::Error(res, "Failed to get album screenshots");
        }
    });
} 