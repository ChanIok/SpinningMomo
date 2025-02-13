#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct WatchedFolder {
    int id{0};
    std::string path;
    std::string last_scan;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WatchedFolder, 
        id, path, last_scan)
};

struct ThumbnailSettings {
    struct {
        int width{200};
        int height{200};
    } size;
    int quality{80};
    std::string storage_location;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ThumbnailSettings, 
        size.width, size.height, quality, storage_location)
};

struct InterfaceSettings {
    std::string theme{"light"};
    std::string language{"zh-CN"};
    std::string default_view_mode{"grid"};
    int grid_columns{4};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(InterfaceSettings,
        theme, language, default_view_mode, grid_columns)
};

struct PerformanceSettings {
    int scan_threads{4};
    int cache_size{1000};
    bool preload_images{true};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PerformanceSettings,
        scan_threads, cache_size, preload_images)
};

struct AppSettings {
    std::string version{"1.0"};
    std::vector<WatchedFolder> watched_folders;
    ThumbnailSettings thumbnails;
    InterfaceSettings interface_settings;
    PerformanceSettings performance;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AppSettings,
        version, watched_folders, thumbnails, interface_settings, performance)
}; 