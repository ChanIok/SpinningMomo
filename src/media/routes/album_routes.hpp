#pragma once

#include <httplib.h>
#include <spdlog/spdlog.h>
#include "../services/service_manager.hpp"

/**
 * @brief 注册相册相关的所有路由
 * @param server HTTP服务器实例
 */
inline void register_album_routes(httplib::Server& server) {
    auto& album_service = AlbumService::get_instance();

    // GET /api/albums - 获取所有相册列表
    server.Get("/api/albums", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto albums = album_service.get_albums();
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& album : albums) {
                json_array.push_back(album.to_json());
            }
            res.set_content(json_array.dump(), "application/json");
            spdlog::info("Successfully retrieved {} albums", albums.size());
        } catch (const std::exception& e) {
            spdlog::error("Failed to get albums: {}", e.what());
            res.status = 500;
            res.set_content(R"({"error":"Internal server error"})", "application/json");
        }
    });

    // POST /api/albums - 创建新相册
    server.Post("/api/albums", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto json = nlohmann::json::parse(req.body);
            auto album = album_service.create_album(
                json["name"].get<std::string>(),
                json.value("description", "")
            );
            res.set_content(album.to_json().dump(), "application/json");
            spdlog::info("Created new album: {}", album.name);
        } catch (const std::exception& e) {
            spdlog::error("Failed to create album: {}", e.what());
            res.status = 400;
            res.set_content(R"({"error":")" + std::string(e.what()) + R"("})", "application/json");
        }
    });

    // GET /api/albums/:id - 获取单个相册详情
    server.Get(R"(/api/albums/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto id = std::stoll(req.matches[1]);
            auto album = album_service.get_album(id);
            res.set_content(album.to_json().dump(), "application/json");
            spdlog::info("Retrieved album {}: {}", id, album.name);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get album {}: {}", req.matches[1].str(), e.what());
            res.status = 404;
            res.set_content(R"({"error":"Album not found"})", "application/json");
        }
    });

    // PUT /api/albums/:id - 更新相册信息
    server.Put(R"(/api/albums/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto id = std::stoll(req.matches[1]);
            auto json = nlohmann::json::parse(req.body);
            auto album = album_service.get_album(id);
            album.name = json["name"].get<std::string>();
            album.description = json.value("description", album.description);
            
            if (album_service.update_album(album)) {
                res.set_content(album.to_json().dump(), "application/json");
                spdlog::info("Updated album {}: {}", id, album.name);
            } else {
                spdlog::error("Album not found for update: {}", id);
                res.status = 404;
                res.set_content(R"({"error":"Album not found"})", "application/json");
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to update album {}: {}", req.matches[1].str(), e.what());
            res.status = 400;
            res.set_content(R"({"error":")" + std::string(e.what()) + R"("})", "application/json");
        }
    });

    // DELETE /api/albums/:id - 删除相册
    server.Delete(R"(/api/albums/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        auto id = std::stoll(req.matches[1]);
        if (album_service.delete_album(id)) {
            spdlog::info("Deleted album {}", id);
            res.status = 204;
        } else {
            spdlog::error("Album not found for deletion: {}", id);
            res.status = 404;
            res.set_content(R"({"error":"Album not found"})", "application/json");
        }
    });
} 