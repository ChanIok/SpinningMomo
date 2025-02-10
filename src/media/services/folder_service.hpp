#pragma once
#include <string>
#include <optional>
#include <filesystem>

class FolderService {
public:
    static FolderService& get_instance();
    
    // 禁止拷贝和移动
    FolderService(const FolderService&) = delete;
    FolderService& operator=(const FolderService&) = delete;
    FolderService(FolderService&&) = delete;
    FolderService& operator=(FolderService&&) = delete;

    // 打开文件夹选择对话框
    std::optional<std::string> show_folder_dialog();
    
    // 验证文件夹权限
    bool validate_folder_access(const std::string& path);

private:
    FolderService() = default;
    ~FolderService() = default;

    // 将Windows路径转换为UTF-8
    std::string normalize_path(const std::wstring& path);
}; 