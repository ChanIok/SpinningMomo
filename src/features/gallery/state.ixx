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

export enum class DirectoryWatchBackend {
  Extended,
  Basic,
  Disabled,
};

export enum class RootAvailability {
  Local,
  RemoteReachable,
  RemoteUnreachable,
};

// 每个 root 独立维护同步状态；Faulted 只暂停该 root，等待用户手动重试。
export enum class WatcherSyncState {
  Healthy,
  Recovering,
  Faulted,
};

export struct FolderWatcherState {
  // 监听的根目录（Gallery 内部规范路径：absolute + lexical normal + generic slash）
  std::filesystem::path root_path;
  // watcher 同步时使用的运行时扫描选项（不包含 ignore_rules）。
  Types::ScanOptions scan_options{};
  // 监听线程只读取该 root 的文件系统通知。
  std::jthread watch_thread;
  // 串行监听线程的启动与停止，避免运行时移除和启动同时改写 jthread。
  std::mutex watch_lifecycle_mutex;
  // stop_requested 关闭该 root 的事件接收与同步；handle 用于取消阻塞的目录读取。
  std::atomic<bool> stop_requested{false};
  std::atomic<void*> directory_handle{nullptr};
  // 置位后拒绝重新配置或启动，直到 stop/join 后从 map 删除。
  std::atomic<bool> removal_in_progress{false};
  // pending_mutex 统一保护待处理队列、调度期限、同步状态和最近错误。
  std::mutex pending_mutex;
  // 可执行变更的防抖截止时间；文件稳定候选各自保存探测期限。
  std::optional<std::chrono::steady_clock::time_point> sync_not_before;
  // 启动恢复期间实时通知只入队，待 USN/全量基线完成后再由全局编排线程消费。
  bool startup_recovery_in_progress = false;
  WatcherSyncState sync_state{WatcherSyncState::Healthy};
  std::optional<std::string> last_sync_error;
  // 启动恢复可能在 Gallery 启动线程中直接执行全量扫描，与全局编排线程串行。
  std::mutex sync_execution_mutex;
  std::atomic<DirectoryWatchBackend> watch_backend{DirectoryWatchBackend::Extended};

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
  struct ManualFileSystemIgnoreEntry {
    int in_flight_count = 0;
    std::chrono::steady_clock::time_point ignore_until{};
  };

  // 缩略图目录路径
  std::filesystem::path thumbnails_directory;

  // 应用关闭时置为 true，用于阻止后台启动恢复继续推进。
  std::atomic<bool> shutdown_requested{false};

  // Gallery 关闭时通知所有扫描尽快停止，避免继续读取大文件或生成缩略图。
  std::stop_source scan_stop_source;

  // 扫描持有共享锁，cleanup 持有独占锁后才能安全释放扫描依赖的运行资源。
  std::shared_mutex scan_lifetime_mutex;

  // 忽略规则变更版本：
  // Repository 每次成功修改 ignore_rules 表后递增；watcher 只比较版本号，
  // 不需要监听具体哪条规则变了。
  std::atomic<std::uint64_t> ignore_rules_version{0};

  // 后台 Gallery 启动初始化线程；cleanup 会先 join，避免和资源释放并发。
  std::jthread startup_initialization_thread;

  // 根目录 watcher 状态直接由 GalleryState 持有（key = Gallery 内部规范路径字符串）。
  std::unordered_map<std::string, FolderWatcherState> folder_watchers;
  std::mutex folder_watchers_mutex;

  // Gallery 只使用一条同步编排线程，依次消费所有 root 的待处理事实。
  std::jthread watcher_sync_thread;
  // 调度代数和条件变量只负责唤醒全局编排线程，不保存具体任务。
  std::mutex watcher_sync_mutex;
  std::condition_variable_any watcher_sync_condition;
  std::uint64_t watcher_sync_generation = 0;

  // 本次运行期的 root 可达性快照。UNC 只在启动时探测一次，不做运行中重连刷新。
  std::unordered_map<std::int64_t, RootAvailability> root_availability_by_id;
  std::unordered_map<std::string, RootAvailability> root_availability_by_path;
  std::mutex root_availability_mutex;

  // 应用主动文件系统操作的 watcher 去重表（key 为大小写归一化后的路径比较键）。
  std::unordered_map<std::wstring, ManualFileSystemIgnoreEntry> manual_file_system_ignore_paths;
  std::mutex manual_file_system_ignore_mutex;

  // 新路径继承同内容最早资产的 Gallery 用户数据后，扩展在同一事务内复制自己的资产数据。
  std::function<std::expected<void, std::string>(std::int64_t, std::int64_t)>
      inherit_asset_data_callback;
};

}  // namespace Features::Gallery::State
