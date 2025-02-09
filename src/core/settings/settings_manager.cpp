#include "settings_manager.hpp"
#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

SettingsManager& SettingsManager::get_instance() {
    static SettingsManager instance;
    return instance;
}

bool SettingsManager::init(const std::string& settings_path) {
    if (is_initialized_) {
        return true;
    }

    settings_path_ = settings_path;
    
    try {
        if (!fs::exists(settings_path_)) {
            if (!create_default_settings()) {
                spdlog::error("Failed to create default settings file: {}", settings_path_);
                return false;
            }
        }
        
        if (!load_settings()) {
            spdlog::error("Failed to load settings from: {}", settings_path_);
            return false;
        }

        is_initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize settings manager: {}", e.what());
        return false;
    }
}

bool SettingsManager::load_settings() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::ifstream file(settings_path_);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json json_data;
        file >> json_data;
        settings_ = json_data.get<AppSettings>();

        return validate_settings();
    } catch (const std::exception& e) {
        spdlog::error("Failed to load settings: {}", e.what());
        return false;
    }
}

bool SettingsManager::save_settings() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        if (!validate_settings()) {
            return false;
        }

        backup_settings();

        nlohmann::json json_data = settings_;
        std::ofstream file(settings_path_);
        if (!file.is_open()) {
            return false;
        }

        file << json_data.dump(4);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save settings: {}", e.what());
        return false;
    }
}

AppSettings SettingsManager::get_settings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_;
}

bool SettingsManager::update_settings(const AppSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = settings;
    return save_settings();
}

std::optional<WatchedFolder> SettingsManager::get_watched_folder(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(settings_.watched_folders.begin(), settings_.watched_folders.end(),
        [&path](const WatchedFolder& folder) { return folder.path == path; });
    
    if (it != settings_.watched_folders.end()) {
        return *it;
    }
    return std::nullopt;
}

const std::vector<WatchedFolder>& SettingsManager::get_watched_folders() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_.watched_folders;
}

const ThumbnailSettings& SettingsManager::get_thumbnail_settings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_.thumbnails;
}

const InterfaceSettings& SettingsManager::get_interface_settings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_.interface_settings;
}

const PerformanceSettings& SettingsManager::get_performance_settings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_.performance;
}

bool SettingsManager::add_watched_folder(const WatchedFolder& folder) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(settings_.watched_folders.begin(), settings_.watched_folders.end(),
        [&folder](const WatchedFolder& existing) { return existing.path == folder.path; });
    
    if (it != settings_.watched_folders.end()) {
        return false;
    }

    settings_.watched_folders.push_back(folder);
    return save_settings();
}

bool SettingsManager::remove_watched_folder(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(settings_.watched_folders.begin(), settings_.watched_folders.end(),
        [&path](const WatchedFolder& folder) { return folder.path == path; });
    
    if (it == settings_.watched_folders.end()) {
        return false;
    }

    settings_.watched_folders.erase(it);
    return save_settings();
}

bool SettingsManager::update_thumbnail_settings(const ThumbnailSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.thumbnails = settings;
    return save_settings();
}

bool SettingsManager::update_interface_settings(const InterfaceSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.interface_settings = settings;
    return save_settings();
}

bool SettingsManager::update_performance_settings(const PerformanceSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.performance = settings;
    return save_settings();
}

bool SettingsManager::create_default_settings() {
    settings_ = AppSettings{};  // 使用默认构造函数中的默认值
    return save_settings();
}

bool SettingsManager::validate_settings() const {
    // TODO: 添加更多验证规则
    if (settings_.version.empty()) {
        return false;
    }

    // 验证缩略图设置
    if (settings_.thumbnails.size.width <= 0 || settings_.thumbnails.size.height <= 0 ||
        settings_.thumbnails.quality <= 0 || settings_.thumbnails.quality > 100) {
        return false;
    }

    // 验证性能设置
    if (settings_.performance.scan_threads <= 0 || settings_.performance.cache_size <= 0) {
        return false;
    }

    return true;
}

bool SettingsManager::backup_settings() const {
    try {
        if (!fs::exists(settings_path_)) {
            return true;
        }

        std::string backup_path = settings_path_ + ".backup";
        fs::copy_file(settings_path_, backup_path, fs::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to backup settings: {}", e.what());
        return false;
    }
} 