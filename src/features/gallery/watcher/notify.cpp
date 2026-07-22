module;

module Features.Gallery.Watcher.Notify;

import std;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Watcher.Sync;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Ignore.Service;
import Features.Gallery.Asset.Repository;
import Utils.Logger;
import Utils.Path;
import <windows.h>;

namespace Features::Gallery::Watcher::Notify {

constexpr size_t kWatchBufferSize = 64 * 1024;

// Extended 能区分文件和目录；Basic 缺乏属性时保留 Unknown，不伪装成文件。
enum class NotificationEntryType {
  File,
  Directory,
  Unknown,
};

// 解析后的目录监听事件
struct ParsedNotification {
  std::filesystem::path path;
  DWORD action = 0;
  NotificationEntryType entry_type = NotificationEntryType::Unknown;
};

struct WatchReadResult {
  bool ok = false;
  DWORD bytes_returned = 0;
  DWORD error = 0;
  State::DirectoryWatchBackend backend = State::DirectoryWatchBackend::Extended;
};

auto build_ignore_key(const std::filesystem::path& path)
    -> std::expected<std::wstring, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize ignore path: " + normalized_result.error());
  }
  return Utils::Path::NormalizeForComparison(normalized_result.value());
}

// 清理已经离开 in-flight 且超过缓冲期的手动操作路径。
auto cleanup_expired_manual_file_system_ignores(Core::State::AppState& app_state) -> void {
  auto now = std::chrono::steady_clock::now();
  std::erase_if(app_state.gallery->manual_file_system_ignore_paths, [now](const auto& pair) {
    const auto& entry = pair.second;
    return entry.in_flight_count <= 0 && entry.ignore_until <= now;
  });
}

// 判断路径是否在应用主动文件系统操作的忽略窗口内。
auto is_path_in_manual_file_system_ignore(Core::State::AppState& app_state,
                                          const std::filesystem::path& path) -> bool {
  auto key_result = build_ignore_key(path);
  if (!key_result) {
    return false;
  }

  std::lock_guard<std::mutex> lock(app_state.gallery->manual_file_system_ignore_mutex);
  cleanup_expired_manual_file_system_ignores(app_state);
  auto it = app_state.gallery->manual_file_system_ignore_paths.find(key_result.value());
  if (it == app_state.gallery->manual_file_system_ignore_paths.end()) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  return it->second.in_flight_count > 0 || it->second.ignore_until > now;
}

// 在 Basic 通知无法识别已消失路径类型时，用目录索引与资产前缀判断是否需要全量对账。
auto unknown_removed_path_requires_full_rescan(Core::State::AppState& app_state,
                                               const ParsedNotification& notification)
    -> std::expected<bool, std::string> {
  auto folder_result = Features::Gallery::Folder::Repository::get_folder_by_path(
      app_state, notification.path.string());
  if (!folder_result) {
    return std::unexpected(folder_result.error());
  }
  if (folder_result->has_value()) {
    return true;
  }

  return Features::Gallery::Asset::Repository::has_assets_under_path_prefix(
      app_state, notification.path.string());
}

auto get_cached_watcher_ignore_rules(Core::State::AppState& app_state,
                                     State::FolderWatcherState& watcher)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string> {
  // 早期过滤只用于减少无效 watcher 工作量，不是最终入库依据；
  // apply_incremental_sync 仍会重新从 DB 加载规则做最终判断。
  const auto current_version =
      app_state.gallery ? app_state.gallery->ignore_rules_version.load(std::memory_order_acquire)
                        : 0;

  {
    std::lock_guard<std::mutex> lock(watcher.pending_mutex);
    // 版本一致说明 Repository 之后没有改过规则，可以直接复用缓存，避免频繁查库。
    if (watcher.cached_ignore_rules.has_value() &&
        watcher.cached_ignore_rules_version == current_version) {
      return watcher.cached_ignore_rules.value();
    }
  }

  auto root_folder_result = Features::Gallery::Folder::Repository::get_folder_by_path(
      app_state, watcher.root_path.string());
  if (!root_folder_result) {
    return std::unexpected("Failed to query root folder: " + root_folder_result.error());
  }

  std::optional<std::int64_t> root_folder_id;
  if (root_folder_result->has_value()) {
    root_folder_id = root_folder_result->value().id;
  }

  auto rules_result =
      Features::Gallery::Ignore::Service::load_ignore_rules(app_state, root_folder_id);
  if (!rules_result) {
    return std::unexpected("Failed to load ignore rules: " + rules_result.error());
  }

  auto rules = std::move(rules_result.value());
  {
    std::lock_guard<std::mutex> lock(watcher.pending_mutex);
    // 缓存一份副本给后续通知批次复用；返回值也用副本，避免调用方持有内部引用。
    watcher.cached_ignore_rules = rules;
    watcher.cached_ignore_rules_version = current_version;
  }

  return rules;
}

auto read_directory_changes(HANDLE directory_handle, std::byte* buffer, DWORD buffer_size,
                            DWORD filter, State::DirectoryWatchBackend backend) -> WatchReadResult {
  WatchReadResult result{
      .ok = false,
      .bytes_returned = 0,
      .error = 0,
      .backend = backend,
  };

  auto read_extended = [&]() -> bool {
    return ReadDirectoryChangesExW(directory_handle, buffer, buffer_size, TRUE, filter,
                                   &result.bytes_returned, nullptr, nullptr,
                                   ReadDirectoryNotifyExtendedInformation) != FALSE;
  };

  auto read_basic = [&]() -> bool {
    return ReadDirectoryChangesW(directory_handle, buffer, buffer_size, TRUE, filter,
                                 &result.bytes_returned, nullptr, nullptr) != FALSE;
  };

  if (backend == State::DirectoryWatchBackend::Extended) {
    if (read_extended()) {
      result.ok = true;
      return result;
    }

    result.error = GetLastError();
    if (result.error == ERROR_INVALID_FUNCTION) {
      // 某些卷不支持 ExW 扩展通知，退回到 W 基础通知继续尝试。
      result.backend = State::DirectoryWatchBackend::Basic;
      result.bytes_returned = 0;
      if (read_basic()) {
        result.ok = true;
      } else {
        result.error = GetLastError();
      }
    }
    return result;
  }

  if (backend == State::DirectoryWatchBackend::Basic) {
    if (read_basic()) {
      result.ok = true;
    } else {
      result.error = GetLastError();
    }
    return result;
  }

  result.error = ERROR_INVALID_FUNCTION;
  return result;
}

// 解析 Windows ReadDirectoryChangesExW 系统调用返回的扩展变更通知缓冲区
auto parse_extended_notification_buffer(const std::filesystem::path& root_path,
                                        const std::byte* buffer, DWORD bytes_returned)
    -> std::vector<ParsedNotification> {
  std::vector<ParsedNotification> parsed_notifications;

  // 通知是链表结构，按 NextEntryOffset 一条条解析。
  size_t offset = 0;
  while (offset < bytes_returned) {
    auto* info = reinterpret_cast<const FILE_NOTIFY_EXTENDED_INFORMATION*>(buffer + offset);
    size_t filename_len = static_cast<size_t>(info->FileNameLength / sizeof(wchar_t));
    std::wstring relative_name(info->FileName, filename_len);

    auto full_path = root_path / std::filesystem::path(relative_name);
    auto normalized_result = Utils::Path::NormalizePath(full_path);
    if (normalized_result) {
      parsed_notifications.push_back(ParsedNotification{
          .path = normalized_result.value(),
          .action = info->Action,
          .entry_type = (info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
                            ? NotificationEntryType::Directory
                            : NotificationEntryType::File,
      });
    } else {
      Logger().warn("Failed to normalize watcher path '{}': {}", full_path.string(),
                    normalized_result.error());
    }

    if (info->NextEntryOffset == 0) {
      break;
    }
    offset += info->NextEntryOffset;
  }

  return parsed_notifications;
}

// 解析 Basic 变更通知：对仍存在的新路径补查类型，已消失路径保留 Unknown。
auto parse_basic_notification_buffer(const std::filesystem::path& root_path,
                                     const std::byte* buffer, DWORD bytes_returned)
    -> std::vector<ParsedNotification> {
  std::vector<ParsedNotification> parsed_notifications;

  size_t offset = 0;
  while (offset < bytes_returned) {
    auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer + offset);
    size_t filename_len = static_cast<size_t>(info->FileNameLength / sizeof(wchar_t));
    std::wstring relative_name(info->FileName, filename_len);

    auto full_path = root_path / std::filesystem::path(relative_name);
    auto normalized_result = Utils::Path::NormalizePath(full_path);
    if (normalized_result) {
      auto entry_type = NotificationEntryType::Unknown;
      std::error_code type_error;
      // ADDED/RENAMED_NEW 通常仍可以访问，补查后空目录也能进入结构对账。
      if (std::filesystem::is_directory(normalized_result.value(), type_error)) {
        entry_type = NotificationEntryType::Directory;
      } else if (!type_error &&
                 std::filesystem::is_regular_file(normalized_result.value(), type_error)) {
        entry_type = NotificationEntryType::File;
      }
      parsed_notifications.push_back(ParsedNotification{
          .path = normalized_result.value(),
          .action = info->Action,
          .entry_type = entry_type,
      });
    } else {
      Logger().warn("Failed to normalize watcher path '{}': {}", full_path.string(),
                    normalized_result.error());
    }

    if (info->NextEntryOffset == 0) {
      break;
    }
    offset += info->NextEntryOffset;
  }

  return parsed_notifications;
}

// 过滤并判断目录影响，再把文件通知转换为内部的 UPSERT/REMOVE 动作。
auto process_watch_notifications(Core::State::AppState& app_state,
                                 State::FolderWatcherState& watcher,
                                 const std::vector<ParsedNotification>& notifications) -> void {
  bool require_full_rescan = false;
  std::vector<ParsedNotification> effective_notifications;
  effective_notifications.reserve(notifications.size());

  for (const auto& notification : notifications) {
    if (is_path_in_manual_file_system_ignore(app_state, notification.path)) {
      Logger().debug("Watcher ignored notification for manual file-system path '{}', action={}",
                     notification.path.string(), notification.action);
      continue;
    }
    effective_notifications.push_back(notification);
  }

  if (effective_notifications.empty()) {
    return;
  }

  if (Sync::is_sync_faulted(watcher)) {
    // Faulted 后仍持续排空系统通知，但不再积累逐文件动作；只保留全量 dirty 标记。
    Sync::request_full_rescan(app_state, watcher);
    return;
  }

  for (const auto& notification : effective_notifications) {
    // Basic 无法识别已消失路径的类型；命中目录索引或资产前缀时按结构变化处理。
    if (notification.entry_type == NotificationEntryType::Unknown &&
        (notification.action == FILE_ACTION_REMOVED ||
         notification.action == FILE_ACTION_RENAMED_OLD_NAME)) {
      auto require_full_scan_result =
          unknown_removed_path_requires_full_rescan(app_state, notification);
      if (!require_full_scan_result) {
        Logger().warn(
            "Failed to inspect unknown Basic watcher change for '{}': {}. Scheduling full rescan.",
            notification.path.string(), require_full_scan_result.error());
        require_full_rescan = true;
        break;
      }

      if (require_full_scan_result.value()) {
        Logger().debug(
            "Unknown Basic watcher change for '{}' affects indexed gallery paths, scheduling full "
            "rescan",
            notification.path.string());
        require_full_rescan = true;
        break;
      }
    }

    if (notification.entry_type != NotificationEntryType::Directory) {
      continue;
    }

    switch (notification.action) {
      case FILE_ACTION_ADDED:
      case FILE_ACTION_REMOVED:
      case FILE_ACTION_RENAMED_OLD_NAME:
      case FILE_ACTION_RENAMED_NEW_NAME: {
        // 目录表现在映射真实目录库存，任何结构变化都需要全量对账。
        Logger().debug(
            "Directory structural change detected for '{}', action={}, scheduling "
            "full rescan",
            notification.path.string(), notification.action);
        require_full_rescan = true;
      } break;
      case FILE_ACTION_MODIFIED:
        Logger().debug(
            "Directory metadata change detected for '{}', action={}, keeping incremental path",
            notification.path.string(), notification.action);
        break;
      default:
        break;
    }

    if (require_full_rescan) {
      break;
    }
  }

  if (require_full_rescan) {
    Sync::request_full_rescan(app_state, watcher);
    return;
  }

  auto options = Sync::get_watcher_scan_options(watcher);
  const auto supported_extensions =
      options.supported_extensions.value_or(Scanner::Common::default_supported_extensions());
  std::optional<std::vector<Types::IgnoreRule>> ignore_rules;
  bool ignore_rules_load_failed = false;
  bool queued_changes = false;

  // 第二轮只处理文件级事件：
  // 目录级影响前面已经判断过；这里再把目录放进文件稳定队列没有意义。
  for (const auto& notification : effective_notifications) {
    auto normalized_path = notification.path.string();

    switch (notification.action) {
      case FILE_ACTION_ADDED:
      case FILE_ACTION_MODIFIED:
      case FILE_ACTION_RENAMED_NEW_NAME: {
        if (notification.entry_type == NotificationEntryType::Directory) {
          break;
        }

        const auto candidate_path = std::filesystem::path(normalized_path);
        // 扩展名过滤比 ignore rules 便宜，先用它拦住 .tmp/.bin 等一定不会入库的文件。
        if (!Scanner::Common::is_supported_file(candidate_path, supported_extensions)) {
          break;
        }

        // 同一个通知批次最多取一次 ignore rules；底层还有版本缓存，通常不会查 DB。
        if (!ignore_rules.has_value() && !ignore_rules_load_failed) {
          auto rules_result = get_cached_watcher_ignore_rules(app_state, watcher);
          if (rules_result) {
            ignore_rules = std::move(rules_result.value());
          } else {
            ignore_rules_load_failed = true;
            Logger().warn("Failed to load watcher ignore rules for '{}': {}",
                          watcher.root_path.string(), rules_result.error());
          }
        }

        // UPSERT 可以早过滤：被忽略的新文件本来就不应该进入图库。
        if (ignore_rules.has_value() &&
            Features::Gallery::Ignore::Service::apply_ignore_rules(
                candidate_path, watcher.root_path, ignore_rules.value(), false)) {
          break;
        }

        // 文件刚出现或仍在写入时，先观察稳定性，再决定是否真正入库。
        Sync::enqueue_file_upsert_for_stability(watcher, normalized_path);
        queued_changes = true;
        break;
      }
      case FILE_ACTION_REMOVED:
      case FILE_ACTION_RENAMED_OLD_NAME:
        if (notification.entry_type == NotificationEntryType::Directory) {
          break;
        }
        // REMOVE 不按 ignore rules 丢弃：
        // 规则可能是在文件入库后才改的，删除事件仍需要机会清理历史数据库记录。
        Sync::enqueue_file_change(watcher, normalized_path, State::PendingFileChangeAction::REMOVE);
        queued_changes = true;
        break;
      default:
        break;
    }
  }

  if (queued_changes) {
    Sync::schedule_sync_task(app_state, watcher);
  }
}

// 目录监听主循环：阻塞读取文件变更，并把可用通知交给 Gallery 全局编排线程。
// 按注册 key 定位一次线程状态；删除流程会取消 IO、join 本线程后再销毁结构体。
auto run_watch_loop(Core::State::AppState& app_state, const std::string& watcher_key,
                    std::stop_token stop_token) -> void {
  State::FolderWatcherState* watcher_ptr = nullptr;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    auto it = app_state.gallery->folder_watchers.find(watcher_key);
    if (it == app_state.gallery->folder_watchers.end()) {
      return;
    }
    watcher_ptr = &it->second;
  }
  auto& watcher = *watcher_ptr;

  auto directory_handle = CreateFileW(watcher.root_path.c_str(), FILE_LIST_DIRECTORY,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (directory_handle == INVALID_HANDLE_VALUE) {
    Logger().error("Failed to open watcher directory '{}', error={}", watcher.root_path.string(),
                   GetLastError());
    return;
  }

  watcher.directory_handle.store(directory_handle, std::memory_order_release);
  Logger().info("Gallery watcher started: {}", watcher.root_path.string());

  std::vector<std::byte> buffer(kWatchBufferSize);
  DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                 FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION |
                 FILE_NOTIFY_CHANGE_SIZE;

  while (!stop_token.stop_requested() && !watcher.stop_requested.load(std::memory_order_acquire)) {
    auto backend = watcher.watch_backend.load(std::memory_order_acquire);
    auto read_result = read_directory_changes(directory_handle, buffer.data(),
                                              static_cast<DWORD>(buffer.size()), filter, backend);

    if (read_result.backend != backend) {
      watcher.watch_backend.store(read_result.backend, std::memory_order_release);
      // 降级后本会话固定使用 Basic，避免 ExW 失败反复刷日志。
      Logger().warn("Watcher backend fallback for '{}': extended -> basic",
                    watcher.root_path.string());
    }

    if (!read_result.ok) {
      auto error = read_result.error;
      if (stop_token.stop_requested() || error == ERROR_OPERATION_ABORTED) {
        break;
      }

      if (error == ERROR_NOTIFY_ENUM_DIR) {
        Logger().warn("Watcher overflow for '{}', scheduling full rescan",
                      watcher.root_path.string());
        Sync::request_full_rescan(app_state, watcher);
        continue;
      }

      if (watcher.watch_backend.load(std::memory_order_acquire) ==
          State::DirectoryWatchBackend::Basic) {
        // 与产品策略一致：W 也不可用时直接停该 watcher，不做额外扫描兜底。
        watcher.watch_backend.store(State::DirectoryWatchBackend::Disabled,
                                    std::memory_order_release);
        Logger().warn(
            "ReadDirectoryChangesW failed for '{}', error={}. Disabling watcher for this session.",
            watcher.root_path.string(), error);
        break;
      }

      Logger().warn("ReadDirectoryChangesExW failed for '{}', error={}", watcher.root_path.string(),
                    error);
      continue;
    }

    if (read_result.bytes_returned == 0) {
      Sync::request_full_rescan(app_state, watcher);
      continue;
    }

    auto notifications =
        watcher.watch_backend.load(std::memory_order_acquire) == State::DirectoryWatchBackend::Basic
            ? parse_basic_notification_buffer(watcher.root_path, buffer.data(),
                                              read_result.bytes_returned)
            : parse_extended_notification_buffer(watcher.root_path, buffer.data(),
                                                 read_result.bytes_returned);
    if (!notifications.empty()) {
      process_watch_notifications(app_state, watcher, notifications);
    }
  }

  watcher.directory_handle.store(nullptr, std::memory_order_release);
  CloseHandle(directory_handle);
  Logger().info("Gallery watcher stopped: {}", watcher.root_path.string());
}

}  // namespace Features::Gallery::Watcher::Notify
