#include "folder_monitor_service.hpp"
#include <spdlog/spdlog.h>

// 全局静态指针
static FolderMonitorService* g_instance = nullptr;

FolderMonitorService& FolderMonitorService::get_instance() {
    if (!g_instance) {
        g_instance = new FolderMonitorService();
    }
    return *g_instance;
}

bool FolderMonitorService::init() {
    try {
        spdlog::info("Initializing folder monitor service...");
        
        // 获取所有监控文件夹并处理
        auto settings = m_settings_manager.get_settings();
        for (const auto& folder : settings.watched_folders) {
            if (!validate_and_process_folder(folder.path)) {
                spdlog::error("Failed to process folder: {}", folder.path);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize folder monitor service: {}", e.what());
        return false;
    }
}

bool FolderMonitorService::add_folder(const std::string& path) {
    try {
        // 验证文件夹访问权限
        if (!m_folder_service.validate_folder_access(path)) {
            spdlog::error("Folder is not accessible: {}", path);
            return false;
        }

        // 检查文件夹是否已存在
        auto settings = m_settings_manager.get_settings();
        for (const auto& folder : settings.watched_folders) {
            if (folder.path == path) {
                spdlog::warn("Folder already exists: {}", path);
                return false;
            }
        }

        // 添加到设置
        WatchedFolder new_folder;
        new_folder.path = path;
        settings.watched_folders.push_back(new_folder);
        
        if (!m_settings_manager.update_settings(settings)) {
            spdlog::error("Failed to update settings");
            return false;
        }

        // 开始处理文件夹
        return validate_and_process_folder(path);
    } catch (const std::exception& e) {
        spdlog::error("Failed to add folder {}: {}", path, e.what());
        return false;
    }
}

bool FolderMonitorService::remove_folder(const std::string& path) {
    try {
        auto settings = m_settings_manager.get_settings();
        auto& folders = settings.watched_folders;
        
        auto it = std::find_if(folders.begin(), folders.end(),
            [&path](const WatchedFolder& folder) { return folder.path == path; });
            
        if (it == folders.end()) {
            spdlog::warn("Folder not found: {}", path);
            return false;
        }

        folders.erase(it);
        return m_settings_manager.update_settings(settings);
    } catch (const std::exception& e) {
        spdlog::error("Failed to remove folder {}: {}", path, e.what());
        return false;
    }
}

bool FolderMonitorService::reprocess_folder(const std::string& path) {
    try {
        // 检查文件夹是否在监控列表中
        auto settings = m_settings_manager.get_settings();
        auto it = std::find_if(settings.watched_folders.begin(), settings.watched_folders.end(),
            [&path](const WatchedFolder& folder) { return folder.path == path; });
            
        if (it == settings.watched_folders.end()) {
            spdlog::error("Folder not in watch list: {}", path);
            return false;
        }

        return validate_and_process_folder(path);
    } catch (const std::exception& e) {
        spdlog::error("Failed to reprocess folder {}: {}", path, e.what());
        return false;
    }
}

ProcessingProgress FolderMonitorService::get_folder_status(const std::string& path) {
    return m_folder_processor.get_progress(path);
}

std::vector<std::pair<std::string, ProcessingProgress>> FolderMonitorService::get_all_folder_status() {
    std::vector<std::pair<std::string, ProcessingProgress>> result;
    auto settings = m_settings_manager.get_settings();
    
    for (const auto& folder : settings.watched_folders) {
        result.emplace_back(folder.path, get_folder_status(folder.path));
    }
    
    return result;
}

bool FolderMonitorService::validate_and_process_folder(const std::string& path) {
    try {
        // 验证文件夹访问权限
        if (!m_folder_service.validate_folder_access(path)) {
            spdlog::error("Folder is not accessible: {}", path);
            return false;
        }

        // 开始处理文件夹
        if (!m_folder_processor.process_folder(path)) {
            spdlog::error("Failed to process folder: {}", path);
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error validating and processing folder {}: {}", path, e.what());
        return false;
    }
}

bool FolderMonitorService::update_folder_settings(const std::string& path) {
    try {
        auto settings = m_settings_manager.get_settings();
        auto it = std::find_if(settings.watched_folders.begin(), settings.watched_folders.end(),
            [&path](const WatchedFolder& folder) { return folder.path == path; });
            
        if (it == settings.watched_folders.end()) {
            return false;
        }

        it->last_scan = std::to_string(std::time(nullptr));
        return m_settings_manager.update_settings(settings);
    } catch (const std::exception& e) {
        spdlog::error("Failed to update folder settings {}: {}", path, e.what());
        return false;
    }
} 