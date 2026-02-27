module;

module Features.Gallery.Watcher;

import std;
import Core.State;
import Core.WorkerPool;
import Core.RPC.NotificationHub;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Scanner;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Service;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Thumbnail;
import Utils.Image;
import Utils.Logger;
import Utils.Path;
import Utils.Time;
import Vendor.BuildConfig;
import Vendor.XXHash;
import <windows.h>;

namespace Features::Gallery::Watcher {

constexpr std::chrono::milliseconds kDebounceDelay{500};
// 监听缓冲区；太小更容易溢出。
constexpr size_t kWatchBufferSize = 64 * 1024;

struct PendingSnapshot {
  // true 走全量，false 按 file_changes 走增量。
  bool require_full_rescan = false;
  // key: 路径，value: 最终动作（同一路径会被合并）
  std::unordered_map<std::string, State::PendingFileChangeAction> file_changes;
};

struct ParsedNotification {
  std::filesystem::path path;
  DWORD action = 0;
  bool is_directory = false;
};

auto to_lowercase(std::string value) -> std::string {
  std::ranges::transform(value, value.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

auto is_supported_file(const std::filesystem::path& file_path,
                       const std::vector<std::string>& supported_extensions) -> bool {
  if (!file_path.has_extension()) {
    return false;
  }

  auto extension = to_lowercase(file_path.extension().string());
  return std::ranges::find(supported_extensions, extension) != supported_extensions.end();
}

auto detect_asset_type(const std::filesystem::path& file_path) -> std::string {
  if (!file_path.has_extension()) {
    return "unknown";
  }

  auto extension = to_lowercase(file_path.extension().string());
  if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp" ||
      extension == ".webp" || extension == ".tiff" || extension == ".tif") {
    return "photo";
  }

  if (extension == ".mp4" || extension == ".avi" || extension == ".mov" || extension == ".mkv" ||
      extension == ".wmv" || extension == ".webm") {
    return "video";
  }

  return "unknown";
}

auto calculate_file_hash(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
  if (Vendor::BuildConfig::is_debug_build()) {
    auto path_str = file_path.string();
    auto hash = std::hash<std::string>{}(path_str);
    return std::format("{:016x}", hash);
  }

  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Cannot open file for hashing: " + file_path.string());
  }

  std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  if (buffer.empty()) {
    return std::unexpected("File is empty: " + file_path.string());
  }

  return Vendor::XXHash::HashCharVectorToHex(buffer);
}

auto make_default_scan_options(const std::filesystem::path& root_path) -> Types::ScanOptions {
  Types::ScanOptions options;
  options.directory = root_path.string();
  return options;
}

auto update_watcher_scan_options(const std::shared_ptr<State::FolderWatcherState>& watcher,
                                 const std::optional<Types::ScanOptions>& scan_options) -> void {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  watcher->scan_options = scan_options.value_or(make_default_scan_options(watcher->root_path));
  watcher->scan_options.directory = watcher->root_path.string();
  // watcher 同步阶段始终以 DB 中已持久化规则为准。
  watcher->scan_options.ignore_rules.reset();
}

auto get_watcher_scan_options(const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> Types::ScanOptions {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  auto options = watcher->scan_options;
  options.directory = watcher->root_path.string();
  options.ignore_rules.reset();
  return options;
}

auto has_pending_changes(const std::shared_ptr<State::FolderWatcherState>& watcher) -> bool {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  return watcher->require_full_rescan || !watcher->pending_file_changes.empty();
}

auto take_pending_snapshot(const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> PendingSnapshot {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);

  PendingSnapshot snapshot;
  snapshot.require_full_rescan = watcher->require_full_rescan;
  snapshot.file_changes = std::move(watcher->pending_file_changes);

  watcher->require_full_rescan = false;

  return snapshot;
}

auto mark_full_rescan(const std::shared_ptr<State::FolderWatcherState>& watcher) -> void {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  // 文件夹有变化时，直接全量扫，逻辑最稳。
  watcher->require_full_rescan = true;
  watcher->pending_file_changes.clear();
  watcher->pending_rescan.store(true, std::memory_order_release);
}

auto queue_file_change(const std::shared_ptr<State::FolderWatcherState>& watcher,
                       const std::string& normalized_path, State::PendingFileChangeAction action)
    -> void {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);

  // 同一路径只留最后一次动作，避免重复处理。
  watcher->pending_file_changes[normalized_path] = action;

  watcher->pending_rescan.store(true, std::memory_order_release);
}

auto parse_notification_buffer(const std::filesystem::path& root_path, const std::byte* buffer,
                               DWORD bytes_returned) -> std::vector<ParsedNotification> {
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
          .is_directory = (info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
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

auto remove_asset_by_path(Core::State::AppState& app_state, const std::filesystem::path& path)
    -> std::expected<bool, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize path: " + normalized_result.error());
  }
  auto normalized = normalized_result.value();

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_path(app_state, normalized.string());
  if (!asset_result) {
    return std::unexpected("Failed to query asset by path: " + asset_result.error());
  }

  if (!asset_result->has_value()) {
    return false;
  }

  auto asset = asset_result->value();
  if (auto thumbnail_result =
          Features::Gallery::Asset::Thumbnail::delete_thumbnail(app_state, asset);
      !thumbnail_result) {
    Logger().debug("Delete thumbnail skipped for '{}': {}", normalized.string(),
                   thumbnail_result.error());
  }

  auto delete_result = Features::Gallery::Asset::Repository::delete_asset(app_state, asset.id);
  if (!delete_result) {
    return std::unexpected("Failed to delete asset: " + delete_result.error());
  }

  return true;
}

auto upsert_asset_by_path(Core::State::AppState& app_state, const std::filesystem::path& root_path,
                          const Types::ScanOptions& options,
                          const std::vector<Types::IgnoreRule>& ignore_rules,
                          const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                          std::optional<Utils::Image::WICFactory>& wic_factory,
                          const std::filesystem::path& path) -> std::expected<int, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize path: " + normalized_result.error());
  }
  auto normalized = normalized_result.value();

  std::error_code ec;
  if (!std::filesystem::exists(normalized, ec) ||
      !std::filesystem::is_regular_file(normalized, ec) || ec) {
    auto remove_result = remove_asset_by_path(app_state, normalized);
    if (!remove_result) {
      return std::unexpected(remove_result.error());
    }
    return 0;
  }

  const auto supported_extensions = options.supported_extensions.value_or(
      std::vector<std::string>{".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff", ".tif"});
  if (!is_supported_file(normalized, supported_extensions)) {
    return 0;
  }

  if (Features::Gallery::Ignore::Service::apply_ignore_rules(normalized, root_path, ignore_rules)) {
    return 0;
  }

  auto file_size = std::filesystem::file_size(normalized, ec);
  if (ec) {
    return std::unexpected("Failed to read file size: " + ec.message());
  }

  auto last_write_time = std::filesystem::last_write_time(normalized, ec);
  if (ec) {
    return std::unexpected("Failed to read file modified time: " + ec.message());
  }

  auto creation_time_result = Utils::Time::get_file_creation_time_millis(normalized);
  if (!creation_time_result) {
    return std::unexpected("Failed to read file creation time: " + creation_time_result.error());
  }

  auto existing_result =
      Features::Gallery::Asset::Repository::get_asset_by_path(app_state, normalized.string());
  if (!existing_result) {
    return std::unexpected("Failed to query existing asset: " + existing_result.error());
  }

  auto existing_asset = existing_result.value();
  auto file_modified_millis = Utils::Time::file_time_to_millis(last_write_time);

  if (existing_asset && existing_asset->size.value_or(0) == static_cast<std::int64_t>(file_size) &&
      existing_asset->file_modified_at.value_or(0) == file_modified_millis) {
    return 0;
  }

  auto hash_result = calculate_file_hash(normalized);
  if (!hash_result) {
    return std::unexpected(hash_result.error());
  }
  auto hash = hash_result.value();

  if (existing_asset && existing_asset->hash.has_value() && !existing_asset->hash->empty() &&
      existing_asset->hash.value() == hash) {
    return 0;
  }

  auto asset_type = detect_asset_type(normalized);

  Types::Asset asset{
      .id = existing_asset ? existing_asset->id : 0,
      .name = normalized.filename().string(),
      .path = normalized.string(),
      .type = asset_type,
      .description = existing_asset ? existing_asset->description : std::nullopt,
      .width = std::nullopt,
      .height = std::nullopt,
      .size = static_cast<std::int64_t>(file_size),
      .extension = std::nullopt,
      .mime_type = "application/octet-stream",
      .hash = std::optional<std::string>{hash},
      .folder_id = std::nullopt,
      .file_created_at = creation_time_result.value(),
      .file_modified_at = file_modified_millis,
      .created_at = existing_asset ? existing_asset->created_at : 0,
      .updated_at = existing_asset ? existing_asset->updated_at : 0,
  };

  if (normalized.has_extension()) {
    asset.extension = to_lowercase(normalized.extension().string());
  }

  auto parent_path = normalized.parent_path().string();
  if (auto folder_it = folder_mapping.find(parent_path); folder_it != folder_mapping.end()) {
    asset.folder_id = folder_it->second;
  }

  if (asset_type == "photo") {
    if (!wic_factory.has_value()) {
      auto wic_result = Utils::Image::get_thread_wic_factory();
      if (wic_result) {
        wic_factory = std::move(wic_result.value());
      } else {
        Logger().warn("Failed to initialize WIC factory for watcher sync: {}", wic_result.error());
      }
    }

    if (wic_factory.has_value()) {
      auto image_info_result = Utils::Image::get_image_info(wic_factory->get(), normalized);
      if (image_info_result) {
        auto image_info = image_info_result.value();
        asset.width = static_cast<std::int32_t>(image_info.width);
        asset.height = static_cast<std::int32_t>(image_info.height);
        asset.mime_type = std::move(image_info.mime_type);
      } else {
        Logger().warn("Failed to read image info for '{}': {}", normalized.string(),
                      image_info_result.error());
        asset.width = 0;
        asset.height = 0;
      }

      if (options.generate_thumbnails.value_or(true)) {
        auto thumbnail_result = Features::Gallery::Asset::Thumbnail::generate_thumbnail(
            app_state, *wic_factory, normalized, hash, options.thumbnail_short_edge.value_or(480));
        if (!thumbnail_result) {
          Logger().warn("Failed to generate thumbnail for '{}': {}", normalized.string(),
                        thumbnail_result.error());
        }
      }
    } else {
      asset.width = 0;
      asset.height = 0;
    }
  } else {
    asset.width = 0;
    asset.height = 0;
  }

  if (existing_asset) {
    auto update_result = Features::Gallery::Asset::Repository::update_asset(app_state, asset);
    if (!update_result) {
      return std::unexpected("Failed to update asset: " + update_result.error());
    }
    return 2;
  }

  auto create_result = Features::Gallery::Asset::Repository::create_asset(app_state, asset);
  if (!create_result) {
    return std::unexpected("Failed to create asset: " + create_result.error());
  }
  return 1;
}

auto apply_incremental_sync(Core::State::AppState& app_state,
                            const std::shared_ptr<State::FolderWatcherState>& watcher,
                            const PendingSnapshot& snapshot)
    -> std::expected<Types::ScanResult, std::string> {
  Types::ScanResult result{};
  auto options = get_watcher_scan_options(watcher);

  auto root_folder_result = Features::Gallery::Folder::Repository::get_folder_by_path(
      app_state, watcher->root_path.string());
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
  auto ignore_rules = std::move(rules_result.value());

  std::vector<std::filesystem::path> upsert_paths;
  upsert_paths.reserve(snapshot.file_changes.size());

  // 先收集 UPSERT 路径，后面执行时也是先删后写，rename 更稳。
  for (const auto& [path, action] : snapshot.file_changes) {
    if (action == State::PendingFileChangeAction::UPSERT) {
      upsert_paths.emplace_back(path);
    }
  }

  std::unordered_map<std::string, std::int64_t> folder_mapping;
  if (!upsert_paths.empty()) {
    auto folder_paths = Features::Gallery::Folder::Service::extract_unique_folder_paths(
        upsert_paths, watcher->root_path);
    auto mapping_result =
        Features::Gallery::Folder::Service::batch_create_folders_for_paths(app_state, folder_paths);
    if (mapping_result) {
      folder_mapping = std::move(mapping_result.value());
    } else {
      Logger().warn("Failed to pre-create folders for incremental sync '{}': {}",
                    watcher->root_path.string(), mapping_result.error());
    }
  }

  std::optional<Utils::Image::WICFactory> wic_factory;

  for (const auto& [path, action] : snapshot.file_changes) {
    if (action != State::PendingFileChangeAction::REMOVE) {
      continue;
    }

    auto remove_result = remove_asset_by_path(app_state, std::filesystem::path(path));
    if (!remove_result) {
      result.errors.push_back(remove_result.error());
      continue;
    }

    if (remove_result.value()) {
      result.deleted_items++;
    }
  }

  for (const auto& [path, action] : snapshot.file_changes) {
    if (action != State::PendingFileChangeAction::UPSERT) {
      continue;
    }

    auto upsert_result =
        upsert_asset_by_path(app_state, watcher->root_path, options, ignore_rules, folder_mapping,
                             wic_factory, std::filesystem::path(path));
    if (!upsert_result) {
      result.errors.push_back(upsert_result.error());
      continue;
    }

    if (upsert_result.value() == 1) {
      result.new_items++;
    } else if (upsert_result.value() == 2) {
      result.updated_items++;
    }
  }

  result.total_files = static_cast<int>(snapshot.file_changes.size());
  result.scan_duration = "incremental";
  return result;
}

auto apply_full_rescan(Core::State::AppState& app_state,
                       const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> std::expected<Types::ScanResult, std::string> {
  auto options = get_watcher_scan_options(watcher);
  return Features::Gallery::Scanner::scan_asset_directory(app_state, options);
}

auto schedule_sync_task(Core::State::AppState& app_state,
                        const std::shared_ptr<State::FolderWatcherState>& watcher) -> void;

auto request_full_rescan(Core::State::AppState& app_state,
                         const std::shared_ptr<State::FolderWatcherState>& watcher) -> void {
  mark_full_rescan(watcher);
  schedule_sync_task(app_state, watcher);
}

auto process_pending_sync(Core::State::AppState& app_state,
                          const std::shared_ptr<State::FolderWatcherState>& watcher) -> void {
  // 每 500ms 合并一次改动，再做一次同步。
  while (!watcher->stop_requested.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(kDebounceDelay);

    watcher->pending_rescan.store(false, std::memory_order_release);
    auto snapshot = take_pending_snapshot(watcher);
    bool has_changes = snapshot.require_full_rescan || !snapshot.file_changes.empty();

    if (!has_changes) {
      if (watcher->pending_rescan.load(std::memory_order_acquire)) {
        continue;
      }
      break;
    }

    auto sync_result = snapshot.require_full_rescan
                           ? apply_full_rescan(app_state, watcher)
                           : apply_incremental_sync(app_state, watcher, snapshot);

    if (!sync_result) {
      Logger().error("Gallery sync failed for '{}': {}", watcher->root_path.string(),
                     sync_result.error());
    } else {
      const auto& result = sync_result.value();
      Logger().info(
          "Gallery sync finished for '{}'. mode={}, total={}, new={}, updated={}, deleted={}, "
          "errors={}",
          watcher->root_path.string(), snapshot.require_full_rescan ? "full" : "incremental",
          result.total_files, result.new_items, result.updated_items, result.deleted_items,
          result.errors.size());

      if (snapshot.require_full_rescan || result.new_items > 0 || result.updated_items > 0 ||
          result.deleted_items > 0) {
        Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
      }
    }

    if (watcher->pending_rescan.load(std::memory_order_acquire) || has_pending_changes(watcher)) {
      continue;
    }

    break;
  }

  watcher->scan_in_progress.store(false, std::memory_order_release);
  if (!watcher->stop_requested.load(std::memory_order_acquire) &&
      (watcher->pending_rescan.load(std::memory_order_acquire) || has_pending_changes(watcher))) {
    schedule_sync_task(app_state, watcher);
  }
}

auto schedule_sync_task(Core::State::AppState& app_state,
                        const std::shared_ptr<State::FolderWatcherState>& watcher) -> void {
  watcher->pending_rescan.store(true, std::memory_order_release);

  bool expected = false;
  // 同一个根目录同一时刻只跑一个同步任务。
  if (!watcher->scan_in_progress.compare_exchange_strong(expected, true,
                                                         std::memory_order_acq_rel)) {
    return;
  }

  bool submitted = Core::WorkerPool::submit_task(*app_state.worker_pool, [&app_state, watcher]() {
    process_pending_sync(app_state, watcher);
  });
  if (!submitted) {
    watcher->scan_in_progress.store(false, std::memory_order_release);
    Logger().warn("Failed to submit gallery sync task for '{}'", watcher->root_path.string());
  }
}

auto process_watch_notifications(Core::State::AppState& app_state,
                                 const std::shared_ptr<State::FolderWatcherState>& watcher,
                                 const std::vector<ParsedNotification>& notifications) -> void {
  bool require_full_rescan = false;

  for (const auto& notification : notifications) {
    if (notification.is_directory) {
      // 文件夹变化直接走全量，避免漏更新。
      require_full_rescan = true;
      break;
    }
  }

  if (require_full_rescan) {
    request_full_rescan(app_state, watcher);
    return;
  }

  for (const auto& notification : notifications) {
    auto normalized_path = notification.path.string();

    switch (notification.action) {
      case FILE_ACTION_ADDED:
      case FILE_ACTION_MODIFIED:
      case FILE_ACTION_RENAMED_NEW_NAME:
        queue_file_change(watcher, normalized_path, State::PendingFileChangeAction::UPSERT);
        break;
      case FILE_ACTION_REMOVED:
      case FILE_ACTION_RENAMED_OLD_NAME:
        queue_file_change(watcher, normalized_path, State::PendingFileChangeAction::REMOVE);
        break;
      default:
        break;
    }
  }

  schedule_sync_task(app_state, watcher);
}

auto run_watch_loop(Core::State::AppState& app_state,
                    const std::shared_ptr<State::FolderWatcherState>& watcher,
                    std::stop_token stop_token) -> void {
  auto directory_handle = CreateFileW(watcher->root_path.c_str(), FILE_LIST_DIRECTORY,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (directory_handle == INVALID_HANDLE_VALUE) {
    Logger().error("Failed to open watcher directory '{}', error={}", watcher->root_path.string(),
                   GetLastError());
    return;
  }

  watcher->directory_handle.store(directory_handle, std::memory_order_release);
  Logger().info("Gallery watcher started: {}", watcher->root_path.string());

  std::vector<std::byte> buffer(kWatchBufferSize);
  DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                 FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION |
                 FILE_NOTIFY_CHANGE_SIZE;

  while (!stop_token.stop_requested() && !watcher->stop_requested.load(std::memory_order_acquire)) {
    DWORD bytes_returned = 0;
    // 这里会阻塞等事件；关闭时用 CancelIoEx 打断。
    BOOL ok = ReadDirectoryChangesExW(
        directory_handle, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE, filter,
        &bytes_returned, nullptr, nullptr, ReadDirectoryNotifyExtendedInformation);

    if (!ok) {
      auto error = GetLastError();
      if (stop_token.stop_requested() || error == ERROR_OPERATION_ABORTED) {
        break;
      }

      if (error == ERROR_NOTIFY_ENUM_DIR) {
        Logger().warn("Watcher overflow for '{}', scheduling full rescan",
                      watcher->root_path.string());
        request_full_rescan(app_state, watcher);
        continue;
      }

      Logger().warn("ReadDirectoryChangesExW failed for '{}', error={}",
                    watcher->root_path.string(), error);
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      continue;
    }

    if (bytes_returned == 0) {
      request_full_rescan(app_state, watcher);
      continue;
    }

    auto notifications =
        parse_notification_buffer(watcher->root_path, buffer.data(), bytes_returned);
    if (!notifications.empty()) {
      process_watch_notifications(app_state, watcher, notifications);
    }
  }

  watcher->directory_handle.store(nullptr, std::memory_order_release);
  CloseHandle(directory_handle);
  Logger().info("Gallery watcher stopped: {}", watcher->root_path.string());
}

auto stop_watcher(const std::shared_ptr<State::FolderWatcherState>& watcher) -> void {
  if (!watcher) {
    return;
  }

  watcher->stop_requested.store(true, std::memory_order_release);

  if (watcher->watch_thread.joinable()) {
    watcher->watch_thread.request_stop();

    auto raw_handle = watcher->directory_handle.load(std::memory_order_acquire);
    auto* directory_handle = static_cast<HANDLE>(raw_handle);
    if (directory_handle && directory_handle != INVALID_HANDLE_VALUE) {
      CancelIoEx(directory_handle, nullptr);
    }

    watcher->watch_thread.join();
  }
}

auto normalize_root_directory(const std::filesystem::path& root_directory)
    -> std::expected<std::filesystem::path, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(root_directory);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize root directory: " + normalized_result.error());
  }

  auto normalized = normalized_result.value();
  if (!std::filesystem::exists(normalized)) {
    return std::unexpected("Root directory does not exist: " + normalized.string());
  }
  if (!std::filesystem::is_directory(normalized)) {
    return std::unexpected("Root path is not a directory: " + normalized.string());
  }

  return normalized;
}

auto ensure_watcher_for_directory(Core::State::AppState& app_state,
                                  const std::filesystem::path& root_directory,
                                  const std::optional<Types::ScanOptions>& scan_options,
                                  bool bootstrap_full_scan) -> std::expected<void, std::string> {
  auto normalized_result = normalize_root_directory(root_directory);
  if (!normalized_result) {
    return std::unexpected(normalized_result.error());
  }
  auto normalized_root = normalized_result.value();
  auto key = normalized_root.string();

  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    if (auto it = app_state.gallery->folder_watchers.find(key);
        it != app_state.gallery->folder_watchers.end()) {
      if (scan_options.has_value()) {
        update_watcher_scan_options(it->second, scan_options);
      }
      // 已经在监听就直接返回。
      return {};
    }
  }

  auto watcher = std::make_shared<State::FolderWatcherState>();
  watcher->root_path = normalized_root;
  update_watcher_scan_options(watcher, scan_options);

  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    auto [it, inserted] = app_state.gallery->folder_watchers.try_emplace(key, watcher);
    if (!inserted) {
      if (scan_options.has_value()) {
        update_watcher_scan_options(it->second, scan_options);
      }
      return {};
    }
  }

  try {
    watcher->watch_thread = std::jthread([&app_state, watcher](std::stop_token stop_token) {
      run_watch_loop(app_state, watcher, stop_token);
    });
  } catch (const std::exception& e) {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    if (auto it = app_state.gallery->folder_watchers.find(key);
        it != app_state.gallery->folder_watchers.end() && it->second == watcher) {
      app_state.gallery->folder_watchers.erase(it);
    }
    return std::unexpected("Failed to start watcher thread: " + std::string(e.what()));
  }

  if (bootstrap_full_scan) {
    // 新建 watcher 后先全量扫一遍，先把数据对齐。
    request_full_rescan(app_state, watcher);
  }
  return {};
}

auto initialize_watchers(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  auto folders_result = Features::Gallery::Folder::Repository::list_all_folders(app_state);
  if (!folders_result) {
    return std::unexpected("Failed to query folders for watcher restore: " +
                           folders_result.error());
  }

  size_t restored_count = 0;
  for (const auto& folder : folders_result.value()) {
    // 只恢复根目录；子目录已经会被递归监听到。
    if (folder.parent_id.has_value()) {
      continue;
    }

    auto start_result = ensure_watcher_for_directory(app_state, std::filesystem::path(folder.path));
    if (!start_result) {
      Logger().warn("Skip watcher restore for '{}': {}", folder.path, start_result.error());
      continue;
    }

    restored_count++;
  }

  Logger().info("Gallery watchers restored: {}", restored_count);
  return {};
}

auto shutdown_watchers(Core::State::AppState& app_state) -> void {
  std::vector<std::shared_ptr<State::FolderWatcherState>> watchers;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    for (auto& [_, watcher] : app_state.gallery->folder_watchers) {
      watchers.push_back(watcher);
    }
    app_state.gallery->folder_watchers.clear();
  }

  for (auto& watcher : watchers) {
    // 先停监听线程，避免退出阶段还在提交同步任务。
    stop_watcher(watcher);
  }

  Logger().info("Gallery watchers stopped: {}", watchers.size());
}

}  // namespace Features::Gallery::Watcher
