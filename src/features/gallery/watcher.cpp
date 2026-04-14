module;

module Features.Gallery.Watcher;

import std;
import Core.State;
import Core.WorkerPool;
import Core.RPC.NotificationHub;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Recovery.Service;
import Features.Gallery.Scanner;
import Features.Gallery.ScanCommon;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Service;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Color.Types;
import Features.Gallery.Color.Extractor;
import Features.Gallery.Color.Repository;
import Utils.Media.VideoAsset;
import Utils.Image;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Utils.Time;
import <windows.h>;

namespace Features::Gallery::Watcher {

constexpr std::chrono::milliseconds kDebounceDelay{500};
constexpr std::chrono::milliseconds kFileStabilityQuietPeriod{2000};
// 手动 move 结束后额外缓冲一段时间，吸收“晚到”的文件系统通知。
constexpr std::chrono::milliseconds kManualMoveIgnoreGracePeriod{3000};
// 监听缓冲区；太小更容易溢出。
constexpr size_t kWatchBufferSize = 64 * 1024;

auto is_shutdown_requested(const Core::State::AppState& app_state) -> bool {
  return app_state.gallery && app_state.gallery->shutdown_requested.load(std::memory_order_acquire);
}

auto build_ignore_key(const std::filesystem::path& path)
    -> std::expected<std::wstring, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize ignore path: " + normalized_result.error());
  }
  return Utils::Path::NormalizeForComparison(normalized_result.value());
}

auto cleanup_expired_manual_move_ignores(Core::State::AppState& app_state) -> void {
  auto now = std::chrono::steady_clock::now();
  std::erase_if(app_state.gallery->manual_move_ignore_paths, [now](const auto& pair) {
    const auto& entry = pair.second;
    return entry.in_flight_count <= 0 && entry.ignore_until <= now;
  });
}

auto is_path_in_manual_move_ignore(Core::State::AppState& app_state,
                                   const std::filesystem::path& path) -> bool {
  auto key_result = build_ignore_key(path);
  if (!key_result) {
    return false;
  }

  std::lock_guard<std::mutex> lock(app_state.gallery->manual_move_ignore_mutex);
  cleanup_expired_manual_move_ignores(app_state);
  auto it = app_state.gallery->manual_move_ignore_paths.find(key_result.value());
  if (it == app_state.gallery->manual_move_ignore_paths.end()) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  return it->second.in_flight_count > 0 || it->second.ignore_until > now;
}

// 待处理变更快照，用于描述需要增量或全量同步的状态
struct PendingSnapshot {
  // true 走全量，false 按 file_changes 走增量。
  bool require_full_rescan = false;
  // key: 路径，value: 最终动作（同一路径会被合并）
  std::unordered_map<std::string, State::PendingFileChangeAction> file_changes;
};

// 解析后的目录监听事件
struct ParsedNotification {
  std::filesystem::path path;
  DWORD action = 0;
  bool is_directory = false;
};

struct ProbedFileState {
  std::int64_t size = 0;
  std::int64_t modified_at = 0;
};

// 生成默认的目录扫描配置
auto make_default_scan_options(const std::filesystem::path& root_path) -> Types::ScanOptions {
  Types::ScanOptions options;
  options.directory = root_path.string();
  return options;
}

// 更新监听器的扫描配置
auto update_watcher_scan_options(const std::shared_ptr<State::FolderWatcherState>& watcher,
                                 const std::optional<Types::ScanOptions>& scan_options) -> void {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  watcher->scan_options = scan_options.value_or(make_default_scan_options(watcher->root_path));
  watcher->scan_options.directory = watcher->root_path.string();
  // watcher 同步阶段始终以 DB 中已持久化规则为准。
  watcher->scan_options.ignore_rules.reset();
}

// 更新扫描完成后的回调函数
auto update_post_scan_callback(const std::shared_ptr<State::FolderWatcherState>& watcher,
                               std::function<void(const Types::ScanResult&)> post_scan_callback)
    -> void {
  if (!post_scan_callback) {
    return;
  }

  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  watcher->post_scan_callback = std::move(post_scan_callback);
}

// 获取监听器配置的扫描完成后回调函数
auto get_post_scan_callback(const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> std::function<void(const Types::ScanResult&)> {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  return watcher->post_scan_callback;
}

// 获取监听器当前的扫描配置
auto get_watcher_scan_options(const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> Types::ScanOptions {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  auto options = watcher->scan_options;
  options.directory = watcher->root_path.string();
  options.ignore_rules.reset();
  return options;
}

auto directory_notification_requires_full_rescan(Core::State::AppState& app_state,
                                                 const ParsedNotification& notification)
    -> std::expected<bool, std::string> {
  switch (notification.action) {
    case FILE_ACTION_ADDED:
      // 运行时新建目录时，先不用急着全量扫描。
      // 真正需要入库的内容，后面还会通过文件通知继续进入增量链路。
      return false;
    case FILE_ACTION_REMOVED:
    case FILE_ACTION_RENAMED_OLD_NAME:
    case FILE_ACTION_RENAMED_NEW_NAME:
      // 目录被删掉或改名时，只有当这个目录下面已经有已入库资产，
      // 才需要 full scan 去重新校正数据库里的路径状态。
      return Features::Gallery::Asset::Repository::has_assets_under_path_prefix(
          app_state, notification.path.string());
    default:
      return false;
  }
}

// 检查是否有待处理的变更（包含全量重扫标记或文件变更）
auto has_pending_changes(const std::shared_ptr<State::FolderWatcherState>& watcher) -> bool {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  return watcher->require_full_rescan || !watcher->pending_file_changes.empty() ||
         !watcher->pending_stable_file_changes.empty();
}

// 获取并清空当前的待处理变更快照
auto take_pending_snapshot(const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> PendingSnapshot {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);

  PendingSnapshot snapshot;
  snapshot.require_full_rescan = watcher->require_full_rescan;
  snapshot.file_changes = std::move(watcher->pending_file_changes);

  watcher->require_full_rescan = false;

  return snapshot;
}

// 标记当前监听器需要执行全量重扫，同时清空原本的增量变更队列
auto mark_full_rescan(const std::shared_ptr<State::FolderWatcherState>& watcher) -> void {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);
  // 文件夹有变化时，直接全量扫，逻辑最稳。
  watcher->require_full_rescan = true;
  watcher->pending_file_changes.clear();
  watcher->pending_stable_file_changes.clear();
  watcher->pending_rescan.store(true, std::memory_order_release);
}

auto probe_file_state(const std::filesystem::path& path)
    -> std::expected<std::optional<ProbedFileState>, std::string> {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    if (ec) {
      return std::unexpected("Failed to check file existence: " + ec.message());
    }
    return std::optional<ProbedFileState>{std::nullopt};
  }

  if (!std::filesystem::is_regular_file(path, ec)) {
    if (ec) {
      return std::unexpected("Failed to check file type: " + ec.message());
    }
    return std::optional<ProbedFileState>{std::nullopt};
  }

  auto file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    return std::unexpected("Failed to read file size: " + ec.message());
  }

  auto last_write_time = std::filesystem::last_write_time(path, ec);
  if (ec) {
    return std::unexpected("Failed to read file modified time: " + ec.message());
  }

  return ProbedFileState{
      .size = static_cast<std::int64_t>(file_size),
      .modified_at = Utils::Time::file_time_to_millis(last_write_time),
  };
}

// 将具体的文件变更动作加入到待处理队列，相同路径的新动作会覆盖之前的
auto queue_file_change(const std::shared_ptr<State::FolderWatcherState>& watcher,
                       const std::string& normalized_path, State::PendingFileChangeAction action)
    -> void {
  std::lock_guard<std::mutex> lock(watcher->pending_mutex);

  // 同一路径只留最后一次动作，避免重复处理。
  watcher->pending_file_changes[normalized_path] = action;
  watcher->pending_stable_file_changes.erase(normalized_path);

  watcher->pending_rescan.store(true, std::memory_order_release);
}

// 将 UPSERT 事件先放入稳定队列，避免录制中的文件被 watcher 当成成品反复分析。
auto stage_file_change_for_stability(const std::shared_ptr<State::FolderWatcherState>& watcher,
                                     const std::string& normalized_path) -> void {
  auto now = std::chrono::steady_clock::now();
  auto probe_result = probe_file_state(std::filesystem::path(normalized_path));

  std::lock_guard<std::mutex> lock(watcher->pending_mutex);

  watcher->pending_file_changes.erase(normalized_path);

  auto& pending = watcher->pending_stable_file_changes[normalized_path];
  pending.action = State::PendingFileChangeAction::UPSERT;
  pending.ready_not_before = now + kFileStabilityQuietPeriod;

  if (!probe_result) {
    Logger().debug("Failed to probe staged gallery file '{}': {}", normalized_path,
                   probe_result.error());
    pending.last_seen_size.reset();
    pending.last_seen_modified_at.reset();
  } else if (probe_result->has_value()) {
    pending.last_seen_size = probe_result->value().size;
    pending.last_seen_modified_at = probe_result->value().modified_at;
  } else {
    pending.last_seen_size.reset();
    pending.last_seen_modified_at.reset();
  }

  watcher->pending_rescan.store(true, std::memory_order_release);
}

// 二次探测到期候选：
// 只有当文件在一个静默窗口后，大小和修改时间都没有继续变化，才提升为真正的 UPSERT。
auto promote_stable_file_changes(const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> void {
  std::vector<std::pair<std::string, State::PendingStableFileChange>> due_candidates;
  auto now = std::chrono::steady_clock::now();

  {
    std::lock_guard<std::mutex> lock(watcher->pending_mutex);
    if (watcher->require_full_rescan) {
      return;
    }

    due_candidates.reserve(watcher->pending_stable_file_changes.size());
    for (const auto& [path, pending] : watcher->pending_stable_file_changes) {
      if (pending.ready_not_before <= now) {
        due_candidates.emplace_back(path, pending);
      }
    }
  }

  for (const auto& [path, pending] : due_candidates) {
    auto probe_result = probe_file_state(std::filesystem::path(path));

    std::lock_guard<std::mutex> lock(watcher->pending_mutex);
    auto it = watcher->pending_stable_file_changes.find(path);
    if (it == watcher->pending_stable_file_changes.end()) {
      continue;
    }

    auto& current = it->second;
    if (current.action != pending.action || current.ready_not_before != pending.ready_not_before) {
      continue;
    }

    if (!probe_result) {
      Logger().debug("Failed to re-probe staged gallery file '{}': {}", path, probe_result.error());
      current.ready_not_before = std::chrono::steady_clock::now() + kFileStabilityQuietPeriod;
      continue;
    }

    if (!probe_result->has_value()) {
      watcher->pending_stable_file_changes.erase(it);
      continue;
    }

    const auto& probed = probe_result->value();
    if (!current.last_seen_size.has_value() || !current.last_seen_modified_at.has_value() ||
        current.last_seen_size.value() != probed.size ||
        current.last_seen_modified_at.value() != probed.modified_at) {
      current.last_seen_size = probed.size;
      current.last_seen_modified_at = probed.modified_at;
      current.ready_not_before = std::chrono::steady_clock::now() + kFileStabilityQuietPeriod;
      continue;
    }

    watcher->pending_file_changes[path] = current.action;
    watcher->pending_stable_file_changes.erase(it);
  }
}

// 解析 Windows ReadDirectoryChangesExW 系统调用返回的变更通知缓冲区
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

// 根据文件路径在数据库中删除对应的资产数据和缩略图
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

// 根据文件路径执行插入或更新，包含读取属性、生成缩略图及提取颜色等全套处理逻辑
// 成功时返回 1（插入）， 2（更新），或 0（由于文件不支持等原因被跳过）
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

  const auto supported_extensions =
      options.supported_extensions.value_or(ScanCommon::default_supported_extensions());
  if (!ScanCommon::is_supported_file(normalized, supported_extensions)) {
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

  auto hash_result = ScanCommon::calculate_file_hash(normalized);
  if (!hash_result) {
    return std::unexpected(hash_result.error());
  }
  auto hash = hash_result.value();

  if (existing_asset && existing_asset->hash.has_value() && !existing_asset->hash->empty() &&
      existing_asset->hash.value() == hash) {
    return 0;
  }

  auto asset_type = ScanCommon::detect_asset_type(normalized);

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
  std::vector<Features::Gallery::Color::Types::ExtractedColor> extracted_colors;

  if (normalized.has_extension()) {
    asset.extension = Utils::String::ToLowerAscii(normalized.extension().string());
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

      auto color_result =
          Features::Gallery::Color::Extractor::extract_main_colors(*wic_factory, normalized);
      if (color_result) {
        extracted_colors = std::move(color_result.value());
      } else {
        Logger().warn("Failed to extract main colors for '{}': {}", normalized.string(),
                      color_result.error());
      }
    } else {
      asset.width = 0;
      asset.height = 0;
      extracted_colors.clear();
    }
  } else if (asset_type == "video") {
    // 与扫描一致：仅元数据 + 可选封面；不写颜色索引（末尾 clear）。
    auto video_result = Utils::Media::VideoAsset::analyze_video_file(
        normalized, options.generate_thumbnails.value_or(true)
                        ? std::optional<std::uint32_t>{options.thumbnail_short_edge.value_or(480)}
                        : std::nullopt);
    if (video_result) {
      asset.width = static_cast<std::int32_t>(video_result->width);
      asset.height = static_cast<std::int32_t>(video_result->height);
      asset.mime_type = video_result->mime_type;

      if (video_result->thumbnail.has_value()) {
        auto thumbnail_result = Features::Gallery::Asset::Thumbnail::save_thumbnail_data(
            app_state, hash, *video_result->thumbnail);
        if (!thumbnail_result) {
          Logger().warn("Failed to save video thumbnail for '{}': {}", normalized.string(),
                        thumbnail_result.error());
        }
      }
    } else {
      return std::unexpected("Failed to analyze video file '" + normalized.string() +
                             "': " + video_result.error());
    }

    extracted_colors.clear();
  } else {
    asset.width = 0;
    asset.height = 0;
    extracted_colors.clear();
  }

  if (existing_asset) {
    auto update_result = Features::Gallery::Asset::Repository::update_asset(app_state, asset);
    if (!update_result) {
      return std::unexpected("Failed to update asset: " + update_result.error());
    }

    auto color_replace_result = Features::Gallery::Color::Repository::replace_asset_colors(
        app_state, asset.id, extracted_colors);
    if (!color_replace_result) {
      return std::unexpected("Failed to update asset colors: " + color_replace_result.error());
    }
    return 2;
  }

  auto create_result = Features::Gallery::Asset::Repository::create_asset(app_state, asset);
  if (!create_result) {
    return std::unexpected("Failed to create asset: " + create_result.error());
  }

  auto color_replace_result = Features::Gallery::Color::Repository::replace_asset_colors(
      app_state, create_result.value(), extracted_colors);
  if (!color_replace_result) {
    return std::unexpected("Failed to create asset colors: " + color_replace_result.error());
  }
  return 1;
}

// 将接收到的待处理快照应用到数据库的最终逻辑（增量模式）
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
  const auto supported_extensions =
      options.supported_extensions.value_or(ScanCommon::default_supported_extensions());

  std::vector<std::filesystem::path> upsert_paths;
  upsert_paths.reserve(snapshot.file_changes.size());

  // 先收集真正可能入库的 UPSERT 路径，避免被忽略或不支持的文件污染 folders 索引。
  for (const auto& [path, action] : snapshot.file_changes) {
    if (action != State::PendingFileChangeAction::UPSERT) {
      continue;
    }

    auto candidate_path = std::filesystem::path(path);
    if (!ScanCommon::is_supported_file(candidate_path, supported_extensions)) {
      continue;
    }

    if (Features::Gallery::Ignore::Service::apply_ignore_rules(candidate_path, watcher->root_path,
                                                               ignore_rules)) {
      continue;
    }

    upsert_paths.emplace_back(std::move(candidate_path));
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
    // 无论索引里是否仍有该行（例如 RPC 已先行删库），都把 REMOVE 写入 changes：
    // ScanChange 表示监视根下的路径已从磁盘消失，供扩展做派生同步（如硬链接撤销）。
    result.changes.push_back(Types::ScanChange{
        .path = path,
        .action = Types::ScanChangeAction::REMOVE,
    });
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
      // NEW / UPDATED 对派生消费者来说都属于“应确保目标状态存在”的 UPSERT。
      result.changes.push_back(Types::ScanChange{
          .path = path,
          .action = Types::ScanChangeAction::UPSERT,
      });
    } else if (upsert_result.value() == 2) {
      result.updated_items++;
      result.changes.push_back(Types::ScanChange{
          .path = path,
          .action = Types::ScanChangeAction::UPSERT,
      });
    }
  }

  result.total_files = static_cast<int>(snapshot.file_changes.size());
  result.scan_duration = "incremental";
  return result;
}

// 执行全量重扫逻辑（直接代理到 Scanner 模块）
auto apply_full_rescan(Core::State::AppState& app_state,
                       const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> std::expected<Types::ScanResult, std::string> {
  auto options = get_watcher_scan_options(watcher);
  auto scan_result = Features::Gallery::Scanner::scan_asset_directory(app_state, options);
  if (!scan_result) {
    return std::unexpected(scan_result.error());
  }

  auto thumbnail_repair_result = Features::Gallery::Asset::Thumbnail::repair_missing_thumbnails(
      app_state, watcher->root_path, options.thumbnail_short_edge.value_or(480));
  if (!thumbnail_repair_result) {
    Logger().warn("Gallery watcher thumbnail repair failed for '{}': {}",
                  watcher->root_path.string(), thumbnail_repair_result.error());
    return scan_result;
  }

  const auto& stats = thumbnail_repair_result.value();
  Logger().info(
      "Gallery watcher thumbnail repair finished for '{}'. context=full_rescan, candidates={}, "
      "missing={}, repaired={}, failed={}, skipped_missing_sources={}",
      watcher->root_path.string(), stats.candidate_hashes, stats.missing_thumbnails,
      stats.repaired_thumbnails, stats.failed_repairs, stats.skipped_missing_sources);
  return scan_result;
}

auto apply_offline_scan_changes(Core::State::AppState& app_state,
                                const std::shared_ptr<State::FolderWatcherState>& watcher,
                                const std::vector<Types::ScanChange>& changes)
    -> std::expected<Types::ScanResult, std::string> {
  // 启动恢复并不重新发明一套“离线同步逻辑”，
  // 而是把 USN 产出的 ScanChange 重新装配成 watcher 已有的增量输入。
  PendingSnapshot snapshot;
  for (const auto& change : changes) {
    snapshot.file_changes[change.path] = change.action == Types::ScanChangeAction::REMOVE
                                             ? State::PendingFileChangeAction::REMOVE
                                             : State::PendingFileChangeAction::UPSERT;
  }

  auto result = apply_incremental_sync(app_state, watcher, snapshot);
  if (!result) {
    return std::unexpected(result.error());
  }

  result->scan_duration = "startup_usn_recovery";
  return result;
}

auto dispatch_scan_result(Core::State::AppState& app_state,
                          const std::shared_ptr<State::FolderWatcherState>& watcher,
                          const Types::ScanResult& result, std::string_view mode,
                          bool force_gallery_changed = false) -> void {
  // 统一收口：日志、gallery.changed 通知、post_scan_callback 都在这里发。
  // 这样启动恢复与运行时增量可以共用同一套“扫描完成后处理”。
  Logger().info(
      "Gallery sync finished for '{}'. mode={}, total={}, new={}, updated={}, deleted={}, "
      "errors={}",
      watcher->root_path.string(), mode, result.total_files, result.new_items, result.updated_items,
      result.deleted_items, result.errors.size());

  if (force_gallery_changed || result.new_items > 0 || result.updated_items > 0 ||
      result.deleted_items > 0 || !result.changes.empty()) {
    Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
  }

  auto post_scan_callback = get_post_scan_callback(watcher);
  if (post_scan_callback && (result.new_items > 0 || result.updated_items > 0 ||
                             result.deleted_items > 0 || !result.changes.empty())) {
    post_scan_callback(result);
  }
}

auto schedule_sync_task(Core::State::AppState& app_state,
                        const std::shared_ptr<State::FolderWatcherState>& watcher) -> void;

// 标记监听器为全量重扫状态并触发下次同步任务
auto request_full_rescan(Core::State::AppState& app_state,
                         const std::shared_ptr<State::FolderWatcherState>& watcher) -> void {
  mark_full_rescan(watcher);
  schedule_sync_task(app_state, watcher);
}

auto run_startup_full_rescan(Core::State::AppState& app_state,
                             const std::shared_ptr<State::FolderWatcherState>& watcher)
    -> std::expected<void, std::string> {
  // 启动阶段的 full scan 需要“当场跑完”，
  // 这样所有 root 的原图/DB 一致性收敛后，才能安全进入后面的全局缩略图缓存对账。
  bool expected = false;
  if (!watcher->scan_in_progress.compare_exchange_strong(expected, true,
                                                         std::memory_order_acq_rel)) {
    return std::unexpected("Startup full rescan skipped: scan task is already in progress");
  }

  mark_full_rescan(watcher);
  watcher->pending_rescan.store(false, std::memory_order_release);
  [[maybe_unused]] auto startup_snapshot = take_pending_snapshot(watcher);

  auto sync_result = apply_full_rescan(app_state, watcher);
  if (sync_result) {
    dispatch_scan_result(app_state, watcher, sync_result.value(), "startup_full", true);
  }

  watcher->scan_in_progress.store(false, std::memory_order_release);
  if (!watcher->stop_requested.load(std::memory_order_acquire) &&
      (watcher->pending_rescan.load(std::memory_order_acquire) || has_pending_changes(watcher))) {
    schedule_sync_task(app_state, watcher);
  }

  if (!sync_result) {
    return std::unexpected(sync_result.error());
  }

  return {};
}

// 核心后台处理循环，具有防抖动合并（Debounce）功能，负责将积攒的变更应用到数据库
auto process_pending_sync(Core::State::AppState& app_state,
                          const std::shared_ptr<State::FolderWatcherState>& watcher) -> void {
  // 每 500ms 合并一次改动，再做一次同步。
  while (!watcher->stop_requested.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(kDebounceDelay);

    watcher->pending_rescan.store(false, std::memory_order_release);
    // 先尝试把“已经安静下来”的候选文件提升到最终增量队列。
    promote_stable_file_changes(watcher);
    auto snapshot = take_pending_snapshot(watcher);
    bool has_executable_changes = snapshot.require_full_rescan || !snapshot.file_changes.empty();

    if (!has_executable_changes) {
      // 这里可能仍有“未稳定”的候选文件，所以不能直接退出 worker。
      if (watcher->pending_rescan.load(std::memory_order_acquire) || has_pending_changes(watcher)) {
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
      dispatch_scan_result(app_state, watcher, sync_result.value(),
                           snapshot.require_full_rescan ? "full" : "incremental",
                           snapshot.require_full_rescan);
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

// 提交异步同步任务到线程池（使用 CAS 保证同一目录同时仅有一个任务在执行）
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

// 将 Windows 系统回调的原始变更通知分类转换为内部的 INSERT/UPDATE/DELETE 动作
auto process_watch_notifications(Core::State::AppState& app_state,
                                 const std::shared_ptr<State::FolderWatcherState>& watcher,
                                 const std::vector<ParsedNotification>& notifications) -> void {
  bool require_full_rescan = false;
  std::vector<ParsedNotification> effective_notifications;
  effective_notifications.reserve(notifications.size());

  for (const auto& notification : notifications) {
    if (is_path_in_manual_move_ignore(app_state, notification.path)) {
      Logger().debug("Watcher ignored notification for manual move path '{}', action={}",
                     notification.path.string(), notification.action);
      continue;
    }
    effective_notifications.push_back(notification);
  }

  if (effective_notifications.empty()) {
    return;
  }

  for (const auto& notification : effective_notifications) {
    if (!notification.is_directory) {
      continue;
    }

    switch (notification.action) {
      case FILE_ACTION_ADDED:
      case FILE_ACTION_REMOVED:
      case FILE_ACTION_RENAMED_OLD_NAME:
      case FILE_ACTION_RENAMED_NEW_NAME: {
        // 目录事件和文件事件不一样：
        // 文件可以直接按单个路径做增量，目录则要先判断会不会波及已有资产。
        auto require_full_scan_result =
            directory_notification_requires_full_rescan(app_state, notification);
        if (!require_full_scan_result) {
          Logger().warn(
              "Failed to inspect directory change impact for '{}': {}. Scheduling full rescan.",
              notification.path.string(), require_full_scan_result.error());
          require_full_rescan = true;
          break;
        }

        if (require_full_scan_result.value()) {
          Logger().debug(
              "Directory structural change detected for '{}', action={}, indexed assets affected, "
              "scheduling full rescan",
              notification.path.string(), notification.action);
          require_full_rescan = true;
        } else {
          Logger().debug(
              "Directory structural change detected for '{}', action={}, no indexed assets "
              "affected, keeping incremental path",
              notification.path.string(), notification.action);
        }
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
    request_full_rescan(app_state, watcher);
    return;
  }

  for (const auto& notification : effective_notifications) {
    auto normalized_path = notification.path.string();

    switch (notification.action) {
      case FILE_ACTION_ADDED:
      case FILE_ACTION_MODIFIED:
      case FILE_ACTION_RENAMED_NEW_NAME:
        // 文件刚出现或仍在写入时，先观察稳定性，再决定是否真正入库。
        stage_file_change_for_stability(watcher, normalized_path);
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

// 目录监听主循环（运行在独立线程中），阻塞调用系统 API 读取文件变更
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

// 停止并清理指定的目录监听器，取消阻塞的系统调用并等待线程退出
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

// 启动指定目录的监听器线程，并处理初始化时的全量重扫请求
auto start_watcher_if_needed(Core::State::AppState& app_state,
                             const std::shared_ptr<State::FolderWatcherState>& watcher,
                             bool bootstrap_full_scan) -> std::expected<bool, std::string> {
  if (!watcher) {
    return false;
  }

  if (watcher->watch_thread.joinable()) {
    return false;
  }

  watcher->stop_requested.store(false, std::memory_order_release);

  try {
    watcher->watch_thread = std::jthread([&app_state, watcher](std::stop_token stop_token) {
      run_watch_loop(app_state, watcher, stop_token);
    });
  } catch (const std::exception& e) {
    return std::unexpected("Failed to start watcher thread: " + std::string(e.what()));
  }

  if (bootstrap_full_scan) {
    request_full_rescan(app_state, watcher);
  }

  return true;
}

// 规范化需要监听的根目录路径，检查是否存在且是目录
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

// 注册一个根目录 watcher；若已存在则更新扫描参数。
auto register_watcher_for_directory(Core::State::AppState& app_state,
                                    const std::filesystem::path& root_directory,
                                    const std::optional<Types::ScanOptions>& scan_options)
    -> std::expected<void, std::string> {
  auto normalized_result = normalize_root_directory(root_directory);
  if (!normalized_result) {
    return std::unexpected(normalized_result.error());
  }
  auto normalized_root = normalized_result.value();
  auto key = normalized_root.string();

  std::shared_ptr<State::FolderWatcherState> watcher;
  bool watcher_created = false;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    if (auto it = app_state.gallery->folder_watchers.find(key);
        it != app_state.gallery->folder_watchers.end()) {
      watcher = it->second;
    } else {
      watcher = std::make_shared<State::FolderWatcherState>();
      watcher->root_path = normalized_root;
      app_state.gallery->folder_watchers.emplace(key, watcher);
      watcher_created = true;
    }
  }

  if (watcher_created || scan_options.has_value()) {
    update_watcher_scan_options(watcher, scan_options);
  }

  return {};
}

// 为已注册的根目录 watcher 设置扫描完成回调。
auto set_post_scan_callback_for_directory(
    Core::State::AppState& app_state, const std::filesystem::path& root_directory,
    std::function<void(const Types::ScanResult&)> post_scan_callback)
    -> std::expected<void, std::string> {
  auto normalized_result = normalize_root_directory(root_directory);
  if (!normalized_result) {
    return std::unexpected(normalized_result.error());
  }

  std::shared_ptr<State::FolderWatcherState> watcher;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    auto it = app_state.gallery->folder_watchers.find(normalized_result->string());
    if (it == app_state.gallery->folder_watchers.end()) {
      return std::unexpected("Watcher is not registered for root directory: " +
                             normalized_result->string());
    }
    watcher = it->second;
  }

  update_post_scan_callback(watcher, std::move(post_scan_callback));
  return {};
}

// 启动一个已注册的根目录 watcher。
auto start_watcher_for_directory(Core::State::AppState& app_state,
                                 const std::filesystem::path& root_directory,
                                 bool bootstrap_full_scan) -> std::expected<void, std::string> {
  auto normalized_result = normalize_root_directory(root_directory);
  if (!normalized_result) {
    return std::unexpected(normalized_result.error());
  }

  std::shared_ptr<State::FolderWatcherState> watcher;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    auto it = app_state.gallery->folder_watchers.find(normalized_result->string());
    if (it == app_state.gallery->folder_watchers.end()) {
      return std::unexpected("Watcher is not registered for root directory: " +
                             normalized_result->string());
    }
    watcher = it->second;
  }

  auto start_result = start_watcher_if_needed(app_state, watcher, bootstrap_full_scan);
  if (!start_result) {
    return std::unexpected(start_result.error());
  }

  return {};
}

// 从程序缓存中移除对应目录的监听器并停止后台线程
auto remove_watcher_for_directory(Core::State::AppState& app_state,
                                  const std::filesystem::path& root_directory)
    -> std::expected<bool, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(root_directory);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize root directory: " + normalized_result.error());
  }

  auto key = normalized_result->string();
  std::shared_ptr<State::FolderWatcherState> watcher;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    if (auto it = app_state.gallery->folder_watchers.find(key);
        it != app_state.gallery->folder_watchers.end()) {
      watcher = it->second;
      app_state.gallery->folder_watchers.erase(it);
    }
  }

  if (!watcher) {
    return false;
  }

  stop_watcher(watcher);
  Logger().info("Gallery watcher removed: {}", key);
  return true;
}

// 在应用启动时，从数据库的文件夹列表中恢复所有的监听器配置
auto restore_watchers_from_db(Core::State::AppState& app_state)
    -> std::expected<void, std::string> {
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

    auto register_result =
        register_watcher_for_directory(app_state, std::filesystem::path(folder.path));
    if (!register_result) {
      Logger().warn("Skip watcher restore for '{}': {}", folder.path, register_result.error());
      continue;
    }

    restored_count++;
  }

  Logger().info("Gallery watcher registrations restored: {}", restored_count);
  return {};
}

// 启动所有已经纳入管理的（注册过的）监听器任务
auto start_registered_watchers(Core::State::AppState& app_state)
    -> std::expected<void, std::string> {
  if (is_shutdown_requested(app_state)) {
    Logger().info("Skip gallery watcher startup recovery: shutdown has been requested");
    return {};
  }

  std::vector<std::shared_ptr<State::FolderWatcherState>> watchers;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    for (auto& [_, watcher] : app_state.gallery->folder_watchers) {
      watchers.push_back(watcher);
    }
  }

  size_t started_count = 0;
  std::optional<std::string> first_error;
  std::uint32_t startup_thumbnail_short_edge = 480;

  // 公共 helper：启动 watcher 线程并统一处理计数和错误记录。
  auto try_start_watcher = [&](const std::shared_ptr<State::FolderWatcherState>& w) -> bool {
    if (is_shutdown_requested(app_state)) {
      Logger().info("Skip watcher start for '{}': shutdown has been requested",
                    w->root_path.string());
      return false;
    }

    auto result = start_watcher_if_needed(app_state, w, false);
    if (!result) {
      Logger().warn("Skip watcher start for '{}': {}", w->root_path.string(), result.error());
      if (!first_error.has_value()) {
        first_error = result.error();
      }
      return false;
    }
    if (result.value()) {
      started_count++;
    }
    return true;
  };

  for (const auto& watcher : watchers) {
    if (is_shutdown_requested(app_state)) {
      Logger().info(
          "Stop gallery watcher startup recovery before '{}': shutdown has been requested",
          watcher->root_path.string());
      break;
    }

    startup_thumbnail_short_edge =
        std::max(startup_thumbnail_short_edge,
                 get_watcher_scan_options(watcher).thumbnail_short_edge.value_or(480));

    // 每个 root 的启动流程：
    // 1. 查询恢复决策（USN 增量 or 全量）
    // 2. USN 模式先补齐离线变更
    // 3. 补完后持久化新检查点
    // 4. 启动实时监听线程
    auto recovery_plan_result = Features::Gallery::Recovery::Service::prepare_startup_recovery(
        app_state, watcher->root_path, get_watcher_scan_options(watcher));
    if (!recovery_plan_result) {
      Logger().warn("Startup recovery decision failed for '{}': {}. Falling back to full scan.",
                    watcher->root_path.string(), recovery_plan_result.error());
      if (!try_start_watcher(watcher)) continue;
      auto startup_full_scan_result = run_startup_full_rescan(app_state, watcher);
      if (!startup_full_scan_result) {
        Logger().warn("Startup full scan failed for '{}': {}", watcher->root_path.string(),
                      startup_full_scan_result.error());
      }
      continue;
    }

    const auto& plan = recovery_plan_result.value();

    if (plan.mode == Features::Gallery::Recovery::Types::StartupRecoveryMode::UsnJournal) {
      Logger().info("Gallery startup recovery for '{}': mode=usn, reason={}, changes={}",
                    watcher->root_path.string(), plan.reason, plan.changes.size());

      if (!plan.changes.empty()) {
        auto recovery_apply_result = apply_offline_scan_changes(app_state, watcher, plan.changes);
        if (!recovery_apply_result) {
          Logger().warn("USN recovery apply failed for '{}': {}. Falling back to full scan.",
                        watcher->root_path.string(), recovery_apply_result.error());
          // apply 失败时仍必须启动 watcher 线程，否则此 root 将无人监听。
          if (!try_start_watcher(watcher)) continue;
          auto startup_full_scan_result = run_startup_full_rescan(app_state, watcher);
          if (!startup_full_scan_result) {
            Logger().warn("Startup full scan failed for '{}': {}", watcher->root_path.string(),
                          startup_full_scan_result.error());
          }
          continue;
        }
        dispatch_scan_result(app_state, watcher, recovery_apply_result.value(), "startup_usn");
      }

      // plan 已携带完整的卷快照信息，直接持久化检查点，无需重新查询 journal。
      Features::Gallery::Recovery::Types::WatchRootRecoveryState recovery_state{
          .root_path = plan.root_path,
          .volume_identity = plan.volume_identity,
          .journal_id = plan.journal_id,
          .checkpoint_usn = plan.checkpoint_usn,
          .rule_fingerprint = plan.rule_fingerprint,
      };
      auto persist_result =
          Features::Gallery::Recovery::Service::persist_recovery_state(app_state, recovery_state);
      if (!persist_result) {
        Logger().warn("Failed to persist startup recovery checkpoint for '{}': {}",
                      watcher->root_path.string(), persist_result.error());
      }

      if (!try_start_watcher(watcher)) continue;
      continue;
    }

    Logger().info("Gallery startup recovery for '{}': mode=full_scan, reason={}",
                  watcher->root_path.string(), plan.reason);

    if (!try_start_watcher(watcher)) continue;
    auto startup_full_scan_result = run_startup_full_rescan(app_state, watcher);
    if (!startup_full_scan_result) {
      Logger().warn("Startup full scan failed for '{}': {}", watcher->root_path.string(),
                    startup_full_scan_result.error());
    }
  }

  // 所有 root 的启动恢复都完成后，统一做一次全局缩略图缓存对账：补 missing、删 orphan。
  auto thumbnail_reconcile_result = Features::Gallery::Asset::Thumbnail::reconcile_thumbnail_cache(
      app_state, startup_thumbnail_short_edge);
  if (!thumbnail_reconcile_result) {
    Logger().warn("Gallery startup thumbnail cache reconcile failed: {}",
                  thumbnail_reconcile_result.error());
  } else {
    const auto& stats = thumbnail_reconcile_result.value();
    Logger().info(
        "Gallery startup thumbnail cache reconcile finished. expected={}, existing={}, "
        "missing={}, repaired={}, orphaned={}, deleted_orphans={}, failed_repairs={}, "
        "failed_orphan_deletions={}, skipped_missing_sources={}",
        stats.expected_hashes, stats.existing_thumbnails, stats.missing_thumbnails,
        stats.repaired_thumbnails, stats.orphaned_thumbnails, stats.deleted_orphaned_thumbnails,
        stats.failed_repairs, stats.failed_orphan_deletions, stats.skipped_missing_sources);
  }

  Logger().info("Gallery watchers started: {} / {}", started_count, watchers.size());
  if (first_error.has_value()) {
    return std::unexpected(first_error.value());
  }
  return {};
}

auto schedule_start_registered_watchers(Core::State::AppState& app_state) -> void {
  if (!app_state.gallery || !app_state.worker_pool) {
    Logger().warn(
        "Skip scheduling gallery watcher startup recovery: gallery state or worker pool is not "
        "ready");
    return;
  }

  if (app_state.gallery->startup_watchers_future.has_value()) {
    Logger().debug("Skip scheduling gallery watcher startup recovery: task is already running");
    return;
  }

  auto promise = std::make_shared<std::promise<void>>();
  app_state.gallery->startup_watchers_future = promise->get_future();

  bool submitted = Core::WorkerPool::submit_task(
      *app_state.worker_pool, [&app_state, promise = std::move(promise)]() mutable {
        auto result = start_registered_watchers(app_state);
        if (!result && !is_shutdown_requested(app_state)) {
          Logger().warn("Gallery watcher startup recovery failed: {}", result.error());
        }
        promise->set_value();
      });

  if (!submitted) {
    app_state.gallery->startup_watchers_future.reset();
    Logger().warn("Skip scheduling gallery watcher startup recovery: worker pool is unavailable");
  }
}

auto wait_for_start_registered_watchers(Core::State::AppState& app_state) -> void {
  if (!app_state.gallery || !app_state.gallery->startup_watchers_future.has_value()) {
    return;
  }
  app_state.gallery->startup_watchers_future->wait();
  app_state.gallery->startup_watchers_future.reset();
}

auto begin_manual_move_ignore(Core::State::AppState& app_state,
                              const std::filesystem::path& source_path,
                              const std::filesystem::path& destination_path)
    -> std::expected<void, std::string> {
  auto source_key_result = build_ignore_key(source_path);
  if (!source_key_result) {
    return std::unexpected(source_key_result.error());
  }
  auto destination_key_result = build_ignore_key(destination_path);
  if (!destination_key_result) {
    return std::unexpected(destination_key_result.error());
  }

  std::lock_guard<std::mutex> lock(app_state.gallery->manual_move_ignore_mutex);
  cleanup_expired_manual_move_ignores(app_state);
  const auto now = std::chrono::steady_clock::now();

  auto touch = [&](const std::wstring& key) {
    // in_flight_count 允许多个并发 move 命中同一路径，避免互相提前解除忽略。
    auto& entry = app_state.gallery->manual_move_ignore_paths[key];
    entry.in_flight_count += 1;
    if (entry.ignore_until < now + kManualMoveIgnoreGracePeriod) {
      entry.ignore_until = now + kManualMoveIgnoreGracePeriod;
    }
  };

  touch(source_key_result.value());
  touch(destination_key_result.value());
  return {};
}

auto complete_manual_move_ignore(Core::State::AppState& app_state,
                                 const std::filesystem::path& source_path,
                                 const std::filesystem::path& destination_path)
    -> std::expected<void, std::string> {
  auto source_key_result = build_ignore_key(source_path);
  if (!source_key_result) {
    return std::unexpected(source_key_result.error());
  }
  auto destination_key_result = build_ignore_key(destination_path);
  if (!destination_key_result) {
    return std::unexpected(destination_key_result.error());
  }

  std::lock_guard<std::mutex> lock(app_state.gallery->manual_move_ignore_mutex);
  cleanup_expired_manual_move_ignores(app_state);
  const auto now = std::chrono::steady_clock::now();

  auto touch = [&](const std::wstring& key) {
    auto it = app_state.gallery->manual_move_ignore_paths.find(key);
    if (it == app_state.gallery->manual_move_ignore_paths.end()) {
      return;
    }

    // 操作完成后并不立刻解除，保留短暂 grace 窗口来过滤延迟事件。
    it->second.in_flight_count = std::max(0, it->second.in_flight_count - 1);
    it->second.ignore_until = now + kManualMoveIgnoreGracePeriod;
  };

  touch(source_key_result.value());
  touch(destination_key_result.value());
  cleanup_expired_manual_move_ignores(app_state);
  return {};
}

// 在应用关闭时，优雅关闭所有的监听器线程和句柄资源
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
