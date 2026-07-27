#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::watcher {

// 从数据库恢复根目录 watcher 注册信息，但不立即启动监听线程。
auto restore_watchers_from_db(core::AppState& app_state) -> std::expected<void, std::string>;

// 注册一个根目录 watcher（重复调用会更新扫描参数，但不会立即启动线程）。
auto register_watcher_for_directory(core::AppState& app_state,
                                    const std::filesystem::path& root_directory,
                                    const std::optional<ScanOptions>& scan_options = std::nullopt)
    -> std::expected<void, std::string>;

// 为已注册的 watcher 设置扫描完成回调。
auto set_post_scan_callback_for_directory(core::AppState& app_state,
                                          const std::filesystem::path& root_directory,
                                          std::function<void(const ScanResult&)> post_scan_callback)
    -> std::expected<void, std::string>;

// 启动一个已注册的 root 监听线程，可选是否调度一次全量扫描。
auto start_watcher_for_directory(core::AppState& app_state,
                                 const std::filesystem::path& root_directory,
                                 bool bootstrap_full_scan = true)
    -> std::expected<void, std::string>;

// 启动所有已注册的 watcher，并在启动后补做一次全量扫描。
auto start_registered_watchers(core::AppState& app_state) -> std::expected<void, std::string>;

// 停止并移除某个目录 watcher。返回 true 表示实际移除了 watcher。
auto remove_watcher_for_directory(core::AppState& app_state,
                                  const std::filesystem::path& root_directory)
    -> std::expected<bool, std::string>;

// 退出时停掉所有 root 监听线程和 Gallery 全局同步编排线程。
auto shutdown_watchers(core::AppState& app_state) -> void;

// 标记应用主动操作的精确源/目标路径进入 watcher 忽略集合，并清除尚未消费的同路径变化。
// begin 成功后调用方才应修改磁盘；已进入媒体分析的任务不在此处强制取消。
auto begin_manual_file_system_ignore(core::AppState& app_state,
                                     const std::filesystem::path& source_path,
                                     const std::filesystem::path& destination_path)
    -> std::expected<void, std::string>;

// 磁盘与索引操作完成后立即结束 in-flight，并保留短缓冲吸收延迟通知。
auto complete_manual_file_system_ignore(core::AppState& app_state,
                                        const std::filesystem::path& source_path,
                                        const std::filesystem::path& destination_path)
    -> std::expected<void, std::string>;

// 判断精确路径是否仍由应用主动操作负责；不递归匹配目录后代。
auto is_path_in_manual_file_system_ignore(core::AppState& app_state,
                                          const std::filesystem::path& path) -> bool;

// 将手动文件操作产出的 ScanChange 分发到对应 root watcher 的 post_scan_callback。
auto dispatch_manual_scan_changes(core::AppState& app_state, const std::vector<ScanChange>& changes)
    -> std::expected<void, std::string>;

}  // namespace features::gallery::watcher
