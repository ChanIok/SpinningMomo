#include "folder_service.hpp"
#include <spdlog/spdlog.h>
#include <windows.h>
#include <shobjidl.h> 
#include <shlwapi.h>
#include <fstream>
#include "media/utils/string_utils.hpp"

FolderService& FolderService::get_instance() {
    static FolderService instance;
    return instance;
}

std::optional<std::string> FolderService::show_folder_dialog() {
    // 初始化COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        spdlog::error("Failed to initialize COM: {}", hr);
        return std::nullopt;
    }

    IFileOpenDialog* pFileDialog;
    std::optional<std::string> result;

    try {
        // 创建FileOpenDialog实例
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileDialog));
        
        if (SUCCEEDED(hr)) {
            // 设置对话框选项
            FILEOPENDIALOGOPTIONS options;
            pFileDialog->GetOptions(&options);
            pFileDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

            // 显示对话框
            hr = pFileDialog->Show(nullptr);
            
            if (SUCCEEDED(hr)) {
                IShellItem* pItem;
                hr = pFileDialog->GetResult(&pItem);
                
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    
                    if (SUCCEEDED(hr)) {
                        // 转换路径为UTF-8
                        result = normalize_path(pszFilePath);
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileDialog->Release();
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in show_folder_dialog: {}", e.what());
    }

    CoUninitialize();
    return result;
}

bool FolderService::validate_folder_access(const std::string& path) {
    try {
        std::filesystem::path fs_path(utf8_to_wide(path));
        
        // 检查路径是否存在
        if (!std::filesystem::exists(fs_path)) {
            return false;
        }

        // 检查是否是目录
        if (!std::filesystem::is_directory(fs_path)) {
            return false;
        }

        // 尝试创建测试文件来验证写入权限
        auto test_file = fs_path / L".test_access";
        std::ofstream file(test_file);
        if (file) {
            file.close();
            std::filesystem::remove(test_file);
            return true;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error validating folder access: {}", e.what());
    }
    return false;
}

std::string FolderService::normalize_path(const std::wstring& path) {
    return wide_to_utf8(path);
} 