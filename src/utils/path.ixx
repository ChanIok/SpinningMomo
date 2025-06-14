module;

#include <windows.h>
#include <Shlwapi.h>

export module Utils.Path;

import std;

// 路径工具命名空间
export namespace Utils::Path {
    // 获取当前程序所在的目录路径
    auto GetExecutableDirectory() -> std::expected<std::filesystem::path, std::string>;
    
    // 获取当前程序的完整路径
    auto GetExecutablePath() -> std::expected<std::filesystem::path, std::string>;
    
    // 确保目录存在，如果不存在则创建
    auto EnsureDirectoryExists(const std::filesystem::path& dir) -> std::expected<void, std::string>;
    
    // 组合路径，类似于 path1 / path2
    auto Combine(const std::filesystem::path& base, const std::string& filename) -> std::filesystem::path;
} 