#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::watcher::sync {

// 更新监听器的扫描配置
auto update_watcher_scan_options(FolderWatcherState& watcher,
                                 const std::optional<ScanOptions>& scan_options) -> void;

// 更新扫描完成后的回调
auto update_post_scan_callback(FolderWatcherState& watcher,
                               std::function<void(const ScanResult&)> post_scan_callback) -> void;

auto get_post_scan_callback(FolderWatcherState& watcher) -> std::function<void(const ScanResult&)>;

auto get_watcher_scan_options(FolderWatcherState& watcher) -> ScanOptions;

// 全量同步失败后，通知层只保留 dirty 状态，不再自动调度扫描。
auto is_sync_faulted(FolderWatcherState& watcher) -> bool;

// 暂停实时队列消费，让启动恢复先建立一致的索引基线。
auto begin_startup_recovery(FolderWatcherState& watcher) -> void;

// 结束启动恢复并唤醒全局编排线程处理期间积累的实时通知。
auto finish_startup_recovery(core::AppState& app_state, FolderWatcherState& watcher) -> void;

// 将文件变更加入最终队列（REMOVE 等立即生效的动作）
auto enqueue_file_change(FolderWatcherState& watcher, const std::string& normalized_path,
                         PendingFileChangeAction action) -> void;

// UPSERT 先进入稳定队列，静默后再提升
auto enqueue_file_upsert_for_stability(FolderWatcherState& watcher,
                                       const std::string& normalized_path) -> void;

// 标记需要全量并调度同步
auto request_full_rescan(core::AppState& app_state, FolderWatcherState& watcher) -> void;

// 更新该 root 的防抖期限，并唤醒 Gallery 全局编排线程。
auto schedule_sync_task(core::AppState& app_state, FolderWatcherState& watcher) -> void;

// Gallery 全局同步编排循环：选择到期 root，串行执行防抖、稳定检测与扫描。
auto run_sync_coordinator(core::AppState& app_state, std::stop_token stop_token) -> void;

// 启动阶段在调用方持有该 root 执行锁时，当场跑完一次全量。
auto run_startup_full_rescan(core::AppState& app_state, FolderWatcherState& watcher)
    -> std::expected<void, std::string>;

// 将 USN 等离线 ScanChange 走增量应用
auto apply_offline_scan_changes(core::AppState& app_state, FolderWatcherState& watcher,
                                const std::vector<ScanChange>& changes)
    -> std::expected<ScanResult, std::string>;

// 统一收口：日志、gallery.changed、post_scan_callback
auto dispatch_scan_result(core::AppState& app_state, FolderWatcherState& watcher,
                          const ScanResult& result, std::string_view mode,
                          bool force_gallery_changed = false) -> void;

}  // namespace features::gallery::watcher::sync
