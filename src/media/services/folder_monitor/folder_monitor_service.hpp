#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "folder_processor.hpp"
#include "core/settings/settings_manager.hpp"
#include "media/services/folder_service.hpp"

class FolderMonitorService {
public:
    static FolderMonitorService& get_instance();

    // 初始化服务
    bool init();

    // 添加监控文件夹
    bool add_folder(const std::string& path);
    
    // 移除监控文件夹
    bool remove_folder(const std::string& path);
    
    // 重新处理指定文件夹
    bool reprocess_folder(const std::string& path);
    
    // 获取文件夹处理状态
    ProcessingProgress get_folder_status(const std::string& path);
    
    // 获取所有监控文件夹状态
    std::vector<nlohmann::json> get_all_folder_status();

private:
    FolderMonitorService() = default;
    ~FolderMonitorService() = default;

    // 禁止拷贝和移动
    FolderMonitorService(const FolderMonitorService&) = delete;
    FolderMonitorService& operator=(const FolderMonitorService&) = delete;
    FolderMonitorService(FolderMonitorService&&) = delete;
    FolderMonitorService& operator=(FolderMonitorService&&) = delete;

    // 验证并处理文件夹
    bool validate_and_process_folder(const std::string& path);
    
    // 更新文件夹状态到设置
    bool update_folder_settings(const std::string& path);

    // 服务依赖
    SettingsManager& m_settings_manager = SettingsManager::get_instance();
    FolderProcessor& m_folder_processor = FolderProcessor::get_instance();
    FolderService& m_folder_service = FolderService::get_instance();
}; 