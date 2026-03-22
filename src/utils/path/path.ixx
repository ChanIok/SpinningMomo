module;

export module Utils.Path;

import std;

// 路径工具命名空间
namespace Utils::Path {

// 应用运行模式
export enum class AppMode {
  Portable,
  Installed,
};

// 获取当前程序所在的目录路径
export auto GetExecutableDirectory() -> std::expected<std::filesystem::path, std::string>;

// 获取当前程序的完整路径
export auto GetExecutablePath() -> std::expected<std::filesystem::path, std::string>;

// 检测当前是否为便携版模式（exe 同目录存在 portable 标记文件）
export auto GetAppMode() -> AppMode;

// 获取应用运行时数据根目录：
// - 便携版：<exe>/data
// - 安装版：%LOCALAPPDATA%/ChanIok/SpinningMomo
export auto GetAppDataDirectory() -> std::expected<std::filesystem::path, std::string>;

// 获取应用运行时数据子目录，并确保目录存在
export auto GetAppDataSubdirectory(std::string_view name)
    -> std::expected<std::filesystem::path, std::string>;

// 获取应用运行时数据文件路径，并确保数据根目录存在
export auto GetAppDataFilePath(std::string_view filename)
    -> std::expected<std::filesystem::path, std::string>;

// 获取内置前端静态资源根目录：<exe>/resources/web
export auto GetEmbeddedWebRootDirectory() -> std::expected<std::filesystem::path, std::string>;

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
