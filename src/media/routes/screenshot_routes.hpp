#pragma once

#include <httplib.h>
#include "../services/service_manager.hpp"

// 注册截图相关路由
inline void register_screenshot_routes(httplib::Server& server) {
    auto& screenshot_service = ScreenshotService::get_instance();

    // 获取相册中的所有截图
    server.Get(R"(/api/albums/(\d+)/screenshots)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto album_id = std::stoll(req.matches[1]);
            auto& album_service = AlbumService::get_instance();
            auto screenshots = album_service.get_album_screenshots(album_id);
            
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& screenshot : screenshots) {
                json_array.push_back(screenshot.to_json());
            }
            res.set_content(json_array.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 404;
            res.set_content(R"({"error":"Album not found"})", "application/json");
        }
    });

    // 获取单张截图
    server.Get(R"(/api/screenshots/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto id = std::stoll(req.matches[1]);
            auto screenshot = screenshot_service.get_screenshot(id);
            res.set_content(screenshot.to_json().dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 404;
            res.set_content(R"({"error":"Screenshot not found"})", "application/json");
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