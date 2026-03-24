module;

export module Features.Gallery.State;

import std;
import Features.Gallery.Types;
import Utils.LRUCache;

namespace Features::Gallery::State {

export enum class PendingFileChangeAction { UPSERT, REMOVE };

export struct PendingStableFileChange {
  // 目前稳定队列只用于延迟 UPSERT；REMOVE 仍然直接落到最终队列。
  PendingFileChangeAction action{PendingFileChangeAction::UPSERT};
  // 最近一次观测到的文件大小和修改时间，用于判断文件是否仍在被写入。
  std::optional<std::int64_t> last_seen_size;
  std::optional<std::int64_t> last_seen_modified_at;
  // 到达这个时间点后才允许再次探测并决定是否提升到最终队列。
  std::chrono::steady_clock::time_point ready_not_before{};
};

export struct FolderWatcherState {
  // 监听的根目录（已规范化）
  std::filesystem::path root_path;
  // watcher 同步时使用的运行时扫描选项（不包含 ignore_rules）。
  Types::ScanOptions scan_options{};
  std::jthread watch_thread;
  std::atomic<bool> scan_in_progress{false};
  std::atomic<bool> pending_rescan{false};
  std::atomic<bool> stop_requested{false};
  std::atomic<void*> directory_handle{nullptr};
  std::mutex pending_mutex;

  // true 表示要做全量扫描（会清空增量列表）
  bool require_full_rescan{false};

  // 待处理的文件改动（同一路径只保留最后一次）
  std::unordered_map<std::string, PendingFileChangeAction> pending_file_changes;

  // 已收到事件，但仍在等待稳定的文件改动候选
  std::unordered_map<std::string, PendingStableFileChange> pending_stable_file_changes;

  // 扫描完成后的回调（可选），由注册方注入，扫描有变化时触发
  std::function<void(const Types::ScanResult&)> post_scan_callback;
};

export struct GalleryState {
  // 缩略图目录路径
  std::filesystem::path thumbnails_directory;

  // 应用关闭时置为 true，用于阻止后台启动恢复继续推进。
  std::atomic<bool> shutdown_requested{false};

  // 后台 watcher 启动恢复任务的 future，shutdown 时等待其结束。
  std::optional<std::future<void>> startup_watchers_future;

  // 原图路径缓存 (asset_id -> filesystem::path)
  Utils::LRUCache::LRUCacheState<std::int64_t, std::filesystem::path> image_path_cache{
      .capacity = 5000, .map = {}, .list = {}};

  // 根目录 watcher 状态（key = 规范化路径字符串）
  std::unordered_map<std::string, std::shared_ptr<FolderWatcherState>> folder_watchers;
  std::mutex folder_watchers_mutex;
};

}  // namespace Features::Gallery::State
