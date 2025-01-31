#pragma once

#include "screenshot_service.hpp"
#include "album_service.hpp"

class ServiceManager {
public:
    static ServiceManager& get_instance() {
        static ServiceManager instance;
        return instance;
    }

    ScreenshotService& get_screenshot_service() { return ScreenshotService::get_instance(); }
    AlbumService& get_album_service() { return AlbumService::get_instance(); }

    // 初始化所有服务
    bool initialize() { return true; }
    // 清理所有服务
    void cleanup() {}

private:
    ServiceManager() = default;
    ~ServiceManager() = default;
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;
}; 