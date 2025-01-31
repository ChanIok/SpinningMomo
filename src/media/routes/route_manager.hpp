#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include "album_routes.hpp"
#include "screenshot_routes.hpp"

/**
 * @brief 路由管理器类
 * 负责注册和管理所有HTTP路由
 */
class RouteManager {
public:
    /**
     * @brief 获取路由管理器单例实例
     * @return 路由管理器实例的引用
     */
    static RouteManager& get_instance() {
        static RouteManager instance;
        return instance;
    }

    /**
     * @brief 注册所有路由到HTTP服务器
     * @param server HTTP服务器实例
     */
    void register_routes(httplib::Server& server) {
        // 注册健康检查路由
        server.Get("/api/health", [](const httplib::Request& req, httplib::Response& res) {
            nlohmann::json response = {
                {"status", "ok"},
                {"timestamp", std::time(nullptr)}
            };
            res.set_content(response.dump(), "application/json");
            spdlog::info("Health check request received");
        });

        // 注册相册和截图相关路由
        register_album_routes(server);
        register_screenshot_routes(server);
        
        spdlog::info("All routes registered successfully");
    }

private:
    // 禁止外部创建实例
    RouteManager() = default;
    ~RouteManager() = default;
    RouteManager(const RouteManager&) = delete;
    RouteManager& operator=(const RouteManager&) = delete;
}; 