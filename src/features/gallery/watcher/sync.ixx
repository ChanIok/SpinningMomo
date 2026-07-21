module;

export module Features.Gallery.Watcher.Sync;

import std;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;

namespace Features::Gallery::Watcher::Sync {

// 更新监听器的扫描配置
export auto update_watcher_scan_options(State::FolderWatcherState& watcher,
                                        const std::optional<Types::ScanOptions>& scan_options)
    -> void;

// 更新扫描完成后的回调
export auto update_post_scan_callback(
    State::FolderWatcherState& watcher,
    std::function<void(const Types::ScanResult&)> post_scan_callback) -> void;

export auto get_post_scan_callback(State::FolderWatcherState& watcher)
    -> std::function<void(const Types::ScanResult&)>;

export auto get_watcher_scan_options(State::FolderWatcherState& watcher) -> Types::ScanOptions;

// 全量同步失败后，通知层只保留 dirty 状态，不再自动调度扫描。
export auto is_sync_faulted(State::FolderWatcherState& watcher) -> bool;

// 暂停实时队列消费，让启动恢复先建立一致的索引基线。
export auto begin_startup_recovery(State::FolderWatcherState& watcher) -> void;

// 结束启动恢复并唤醒全局编排线程处理期间积累的实时通知。
export auto finish_startup_recovery(Core::State::AppState& app_state,
                                    State::FolderWatcherState& watcher) -> void;

// 将文件变更加入最终队列（REMOVE 等立即生效的动作）
export auto enqueue_file_change(State::FolderWatcherState& watcher,
                                const std::string& normalized_path,
                                State::PendingFileChangeAction action) -> void;

// UPSERT 先进入稳定队列，静默后再提升
export auto enqueue_file_upsert_for_stability(State::FolderWatcherState& watcher,
                                              const std::string& normalized_path) -> void;

// 标记需要全量并调度同步
export auto request_full_rescan(Core::State::AppState& app_state,
                                State::FolderWatcherState& watcher) -> void;

// 更新该 root 的防抖期限，并唤醒 Gallery 全局编排线程。
export auto schedule_sync_task(Core::State::AppState& app_state, State::FolderWatcherState& watcher)
    -> void;

// Gallery 全局同步编排循环：选择到期 root，串行执行防抖、稳定检测与扫描。
export auto run_sync_coordinator(Core::State::AppState& app_state, std::stop_token stop_token)
    -> void;

// 启动阶段在调用方持有该 root 执行锁时，当场跑完一次全量。
export auto run_startup_full_rescan(Core::State::AppState& app_state,
                                    State::FolderWatcherState& watcher)
    -> std::expected<void, std::string>;

// 将 USN 等离线 ScanChange 走增量应用
export auto apply_offline_scan_changes(Core::State::AppState& app_state,
                                       State::FolderWatcherState& watcher,
                                       const std::vector<Types::ScanChange>& changes)
    -> std::expected<Types::ScanResult, std::string>;

// 统一收口：日志、gallery.changed、post_scan_callback
export auto dispatch_scan_result(Core::State::AppState& app_state,
                                 State::FolderWatcherState& watcher,
                                 const Types::ScanResult& result, std::string_view mode,
                                 bool force_gallery_changed = false) -> void;

}  // namespace Features::Gallery::Watcher::Sync
