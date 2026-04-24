module;

export module Features.Gallery.State;

import std;
import Features.Gallery.Types;

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

  // watcher 早期过滤使用的忽略规则缓存：
  // 文件系统事件可能非常密集，如果每个通知批次都查 DB 加载规则，会把“忽略目录里的
  // 大量无关文件变动”变成数据库压力。这里缓存最近一次加载结果，版本变化时再失效。
  std::optional<std::vector<Types::IgnoreRule>> cached_ignore_rules;
  std::uint64_t cached_ignore_rules_version = 0;

  // 扫描完成后的回调（可选），由注册方注入，扫描有变化时触发
  std::function<void(const Types::ScanResult&)> post_scan_callback;
};

export struct GalleryState {
  struct ManualMoveIgnoreEntry {
    int in_flight_count = 0;
    std::chrono::steady_clock::time_point ignore_until{};
  };

  // 缩略图目录路径
  std::filesystem::path thumbnails_directory;

  // 应用关闭时置为 true，用于阻止后台启动恢复继续推进。
  std::atomic<bool> shutdown_requested{false};

  // 忽略规则变更版本：
  // Repository 每次成功修改 ignore_rules 表后递增；watcher 只比较版本号，
  // 不需要监听具体哪条规则变了。
  std::atomic<std::uint64_t> ignore_rules_version{0};

  // 后台 watcher 启动恢复任务的 future，shutdown 时等待其结束。
  std::optional<std::future<void>> startup_watchers_future;

  // 根目录 watcher 状态（key = 规范化路径字符串）
  std::unordered_map<std::string, std::shared_ptr<FolderWatcherState>> folder_watchers;
  std::mutex folder_watchers_mutex;

  // 手动 move 操作的 watcher 去重表（key 为大小写归一化后的路径比较键）。
  std::unordered_map<std::wstring, ManualMoveIgnoreEntry> manual_move_ignore_paths;
  std::mutex manual_move_ignore_mutex;
};

}  // namespace Features::Gallery::State
