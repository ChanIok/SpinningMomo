#pragma once

#include <mutex>
#include <string>
#include <memory>
#include <optional>
#include "settings_types.hpp"

class SettingsManager {
public:
    static SettingsManager& get_instance();

    // 禁止拷贝和移动
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    SettingsManager(SettingsManager&&) = delete;
    SettingsManager& operator=(SettingsManager&&) = delete;

    // 初始化设置管理器
    bool init(const std::string& settings_path);

    // 加载和保存设置
    bool load_settings();
    bool save_settings();

    // 获取设置
    AppSettings get_settings() const;
    
    // 更新设置
    bool update_settings(const AppSettings& settings);

    // 获取单个设置项
    std::optional<WatchedFolder> get_watched_folder(const std::string& path) const;
    const std::vector<WatchedFolder>& get_watched_folders() const;
    const ThumbnailSettings& get_thumbnail_settings() const;
    const InterfaceSettings& get_interface_settings() const;
    const PerformanceSettings& get_performance_settings() const;

    // 更新单个设置项
    bool add_watched_folder(const WatchedFolder& folder);
    bool remove_watched_folder(const std::string& path);
    bool update_thumbnail_settings(const ThumbnailSettings& settings);
    bool update_interface_settings(const InterfaceSettings& settings);
    bool update_performance_settings(const PerformanceSettings& settings);

private:
    SettingsManager() = default;
    ~SettingsManager() = default;

    bool create_default_settings();
    bool validate_settings() const;
    bool backup_settings() const;
    bool persist_to_file(const nlohmann::json& json_data);

    std::string settings_path_;
    AppSettings settings_;
    mutable std::mutex memory_mutex_;    // 保护内存数据
    mutable std::mutex file_mutex_;      // 保护文件操作
    bool is_initialized_{false};
}; 