#pragma once

#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>
#include "album_routes.hpp"
#include "screenshot_routes.hpp"
#include "settings_routes.hpp"
#include "media/utils/response.hpp"

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
     * @param app uWebSockets应用实例
     */
    void register_routes(uWS::App& app) {
        // 注册健康检查路由
        app.get("/api/health", [](auto* res, auto* req) {
            nlohmann::json response = {
                {"status", "ok"},
                {"timestamp", std::time(nullptr)}
            };
            Response::Success(res, response);
        });

        // 注册相册和截图相关路由
        register_album_routes(app);
        register_screenshot_routes(app);
        
        // 注册设置相关路由
        register_settings_routes(app);
    }

private:
    // 禁止外部创建实例
    RouteManager() = default;
    ~RouteManager() = default;
    RouteManager(const RouteManager&) = delete;
    RouteManager& operator=(const RouteManager&) = delete;
}; 