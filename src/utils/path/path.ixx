module;

export module Utils.Path;

import std;

// 路径工具命名空间
namespace Utils::Path {

// 获取当前程序所在的目录路径
export auto GetExecutableDirectory() -> std::expected<std::filesystem::path, std::string>;

// 获取当前程序的完整路径
export auto GetExecutablePath() -> std::expected<std::filesystem::path, std::string>;

// 确保目录存在，如果不存在则创建
export auto EnsureDirectoryExists(const std::filesystem::path& dir)
    -> std::expected<void, std::string>;

// 组合路径，类似于 path1 / path2
export auto Combine(const std::filesystem::path& base, const std::string& filename)
    -> std::filesystem::path;

// 规范化路径为绝对路径，默认相对于程序目录
export auto NormalizePath(const std::filesystem::path& path,
                          std::optional<std::filesystem::path> base = std::nullopt)
    -> std::expected<std::filesystem::path, std::string>;

// 获取用户视频文件夹路径 (FOLDERID_Videos)
export auto GetUserVideosDirectory() -> std::expected<std::filesystem::path, std::string>;

// 获取应用输出目录：
// 1. 使用配置目录（非空时）
// 2. 回退到 Videos/SpinningMomo
// 3. 最终回退到 exe 目录下的 SpinningMomo
export auto GetOutputDirectory(const std::string& configured_output_dir_path)
    -> std::expected<std::filesystem::path, std::string>;

}  // namespace Utils::Path
