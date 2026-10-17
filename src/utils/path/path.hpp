#pragma once

#include "vendor/std.hpp"

// 路径工具命名空间
namespace utils::path {

// 应用运行模式
enum class AppMode {
  Portable,
  Installed,
};

// 路径所在存储的类别。Remote 表示网络位置，包括 UNC 路径和映射到盘符的网络驱动器。
enum class PathStorageKind {
  Local,
  Remote,
};

// 获取当前程序所在的目录路径
auto GetExecutableDirectory() -> std::expected<std::filesystem::path, std::string>;

// 获取当前程序的完整路径
auto GetExecutablePath() -> std::expected<std::filesystem::path, std::string>;

// 检测当前是否为便携版模式（exe 同目录存在 portable 标记文件）
auto GetAppMode() -> AppMode;

// 获取应用运行时数据根目录：
// - 便携版：<exe>/data
// - 安装版：%LOCALAPPDATA%/ChanIok/SpinningMomo
auto GetAppDataDirectory() -> std::expected<std::filesystem::path, std::string>;

// 获取应用运行时数据子目录，并确保目录存在
auto GetAppDataSubdirectory(std::string_view name)
    -> std::expected<std::filesystem::path, std::string>;

// 获取应用运行时数据文件路径，并确保数据根目录存在
auto GetAppDataFilePath(std::string_view filename)
    -> std::expected<std::filesystem::path, std::string>;

// 获取内置前端静态资源根目录：<exe>/resources/web
auto GetEmbeddedWebRootDirectory() -> std::expected<std::filesystem::path, std::string>;

// 确保目录存在，如果不存在则创建
auto EnsureDirectoryExists(const std::filesystem::path& dir) -> std::expected<void, std::string>;

// 解析边界输入路径为绝对路径，默认相对于程序目录。
// 会访问文件系统以解析现有路径段；适用于用户输入、配置输入、
// watcher/recovery root 等需要拿到真实文件系统语义的入口。
auto ResolvePath(const std::filesystem::path& path,
                 std::optional<std::filesystem::path> base = std::nullopt)
    -> std::expected<std::filesystem::path, std::string>;

// 纯 lexical 路径规范化：不访问文件系统，统一为正斜杠绝对路径。
// 适用于图库内部已知路径语义（DB path / watcher path / scan change path /
// relative 推导等）；需要解析 symlink/盘符映射时请用 ResolvePath。
auto NormalizePath(const std::filesystem::path& path,
                   std::optional<std::filesystem::path> base = std::nullopt)
    -> std::expected<std::filesystem::path, std::string>;

// 从 UNC 路径中提取 server 名称；支持 \\server\share 与 \\?\UNC\server\share。
// 纯字符串解析，不访问文件系统或网络提供者。
auto TryParseUncServer(const std::filesystem::path& path) -> std::optional<std::wstring>;

// 解析路径所在网络位置的 server 名称，覆盖两种网络位置形态：
// - UNC 路径：直接按语法解析；
// - 映射网络驱动器（如 Z:\Photos）：语法上是普通盘符，需要向网络提供者查询盘符对应的
//   UNC 目标后再解析。
// 当前登录会话看不到该盘符映射时（例如进程已提权）返回 nullopt，此时无法判定为网络位置。
auto TryResolveNetworkServer(const std::filesystem::path& path) -> std::optional<std::wstring>;

// 判断路径位于本地存储还是网络位置。会向网络提供者查询盘符映射，不是纯字符串判断。
auto ClassifyPathStorageKind(const std::filesystem::path& path) -> PathStorageKind;

// 获取用户视频文件夹路径 (FOLDERID_Videos)
auto GetUserVideosDirectory() -> std::expected<std::filesystem::path, std::string>;

// 获取应用输出目录：
// 1. 使用配置目录（非空时）
// 2. 回退到 Videos/SpinningMomo
// 3. 最终回退到 exe 目录下的 SpinningMomo
auto GetOutputDirectory(const std::string& configured_output_dir_path)
    -> std::expected<std::filesystem::path, std::string>;

// 获取按窗口标题分类的输出目录，并把标题转换为合法、稳定的 Windows 路径段。
auto GetOutputDirectoryForWindowTitle(const std::string& configured_output_dir_path,
                                      std::wstring_view window_title)
    -> std::expected<std::filesystem::path, std::string>;

// 把路径转成适合比较的统一形式（小写 + 正斜杠）。
// 主要用于 Windows 大小写不敏感的前缀匹配场景。
auto NormalizeForComparison(const std::filesystem::path& path) -> std::wstring;

// 判断 target 是否位于 base 目录内部（大小写不敏感）。
auto IsPathWithinBase(const std::filesystem::path& target, const std::filesystem::path& base)
    -> bool;

}  // namespace utils::path
