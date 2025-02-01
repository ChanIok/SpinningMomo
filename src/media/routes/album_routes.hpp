#pragma once

#include <uwebsockets/App.h>
#include <spdlog/spdlog.h>
#include "media/utils/logger.hpp"
#include "media/utils/response.hpp"
#include "media/utils/request.hpp"
#include "../services/service_manager.hpp"

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
                json_array.push_back(album.to_json());
            }
            Response::Success(res, json_array);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get albums: {}", e.what());
            Response::Error(res, "Internal server error", 500);
        }
    });

    // POST /api/albums - 创建新相册
    app.post("/api/albums", [&](auto* res, auto* req) {
        res->onData([res, &album_service](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto album = album_service.create_album(
                    json["name"].get<std::string>(),
                    json.value("description", "")
                );
                Response::Success(res, album.to_json());
            } catch (const std::exception& e) {
                spdlog::error("Failed to create album: {}", e.what());
                Response::Error(res, e.what());
            }
        });
    });

    // GET /api/albums/:id - 获取单个相册详情
    app.get("/api/albums/:id", [&](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            auto album = album_service.get_album(id);
            Response::Success(res, album.to_json());
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
                    Response::Success(res, album.to_json());
                } else {
                    Response::Error(res, "Album not found", 404);
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to update album: {}", e.what());
                Response::Error(res, e.what());
            }
        });
    });

    // DELETE /api/albums/:id - 删除相册
    app.del("/api/albums/:id", [&](auto* res, auto* req) {
        try {
            auto id = std::stoll(Request::GetPathParam(req, 0));
            if (album_service.delete_album(id)) {
                Response::NoContent(res);
            } else {
                Response::Error(res, "Album not found", 404);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to delete album: {}", e.what());
            Response::Error(res, e.what());
        }
    });
} 