module;

export module Features.Gallery.State;

import std;
import Features.Gallery.Types;
import Utils.LRUCache;

namespace Features::Gallery::State {

export enum class PendingFileChangeAction { UPSERT, REMOVE };

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
};

export struct GalleryState {
  // 缩略图目录路径
  std::filesystem::path thumbnails_directory;

  // 原图路径缓存 (asset_id -> filesystem::path)
  Utils::LRUCache::LRUCacheState<std::int64_t, std::filesystem::path> image_path_cache{
      .capacity = 5000, .map = {}, .list = {}};

  // 根目录 watcher 状态（key = 规范化路径字符串）
  std::unordered_map<std::string, std::shared_ptr<FolderWatcherState>> folder_watchers;
  std::mutex folder_watchers_mutex;
};

}  // namespace Features::Gallery::State
