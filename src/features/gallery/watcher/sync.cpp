module;

module Features.Gallery.Watcher.Sync;

import std;
import Core.State;
import Core.I18n.State;
import Core.Notifications;
import Core.Notifications.Types;
import Core.RPC.NotificationHub;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Scanner;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Scanner.AssetPipeline;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Service;
import Features.Gallery.Asset.Thumbnail;
import Utils.Logger;
import Utils.String;
import Utils.Time;

namespace Features::Gallery::Watcher::Sync {

constexpr std::chrono::milliseconds kDebounceDelay{500};
constexpr std::chrono::milliseconds kFileStabilityQuietPeriod{2000};

auto schedule_sync_task(Core::State::AppState& app_state, State::FolderWatcherState& watcher)
    -> void;

// 待处理变更快照
struct PendingSnapshot {
  bool require_full_rescan = false;
  std::unordered_map<std::string, State::PendingFileChangeAction> file_changes;
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
auto update_watcher_scan_options(State::FolderWatcherState& watcher,
                                 const std::optional<Types::ScanOptions>& scan_options) -> void {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  watcher.scan_options = scan_options.value_or(make_default_scan_options(watcher.root_path));
  watcher.scan_options.directory = watcher.root_path.string();
  // watcher 同步阶段始终以 DB 中已持久化规则为准。
  watcher.scan_options.ignore_rules.reset();
  watcher.cached_ignore_rules.reset();
  watcher.cached_ignore_rules_version = 0;
}

// 更新扫描完成后的回调函数
auto update_post_scan_callback(State::FolderWatcherState& watcher,
                               std::function<void(const Types::ScanResult&)> post_scan_callback)
    -> void {
  if (!post_scan_callback) {
    return;
  }

  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  watcher.post_scan_callback = std::move(post_scan_callback);
}

// 获取监听器配置的扫描完成后回调函数
auto get_post_scan_callback(State::FolderWatcherState& watcher)
    -> std::function<void(const Types::ScanResult&)> {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  return watcher.post_scan_callback;
}

// 获取监听器当前的扫描配置
auto get_watcher_scan_options(State::FolderWatcherState& watcher) -> Types::ScanOptions {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  auto options = watcher.scan_options;
  options.directory = watcher.root_path.string();
  options.ignore_rules.reset();
  return options;
}

// 判断当前 root 是否已暂停自动同步。
auto is_sync_faulted(State::FolderWatcherState& watcher) -> bool {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  return watcher.sync_state == State::WatcherSyncState::Faulted;
}

// 暂停实时队列消费，让启动恢复先建立一致的索引基线。
auto begin_startup_recovery(State::FolderWatcherState& watcher) -> void {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  watcher.startup_recovery_in_progress = true;
}

// 结束启动恢复并唤醒全局编排线程处理期间积累的实时通知。
auto finish_startup_recovery(Core::State::AppState& app_state, State::FolderWatcherState& watcher)
    -> void {
  {
    std::lock_guard<std::mutex> lock(watcher.pending_mutex);
    watcher.startup_recovery_in_progress = false;
  }
  schedule_sync_task(app_state, watcher);
}

// 将一次成功同步收敛为 Healthy，并清除上次错误。
auto mark_sync_healthy(State::FolderWatcherState& watcher) -> void {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  watcher.sync_state = State::WatcherSyncState::Healthy;
  watcher.last_sync_error.reset();
}

// 将增量失败升级为一次全量恢复，并丢弃已经不再可信的逐文件动作。
auto prepare_full_recovery(State::FolderWatcherState& watcher, const std::string& error) -> void {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  watcher.sync_state = State::WatcherSyncState::Recovering;
  watcher.last_sync_error = error;
  watcher.require_full_rescan = true;
  watcher.pending_file_changes.clear();
  watcher.pending_stable_file_changes.clear();
}

// 将全量失败收敛为 Faulted，只保留后续必须全量对账的事实。
auto mark_sync_faulted(State::FolderWatcherState& watcher, const std::string& error) -> void {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  watcher.sync_state = State::WatcherSyncState::Faulted;
  watcher.last_sync_error = error;
  // Faulted 期间不再保存逐文件动作，只保留“必须全量对账”的事实。
  watcher.require_full_rescan = true;
  watcher.pending_file_changes.clear();
  watcher.pending_stable_file_changes.clear();
}

// 响应用户重试：把 Faulted 切回 Recovering，并唤醒一次全量同步。
// 根据当前注册表响应延迟到达的用户重试，已删除或正在停止的 root 直接忽略。
auto retry_faulted_sync(Core::State::AppState& app_state, const std::string& watcher_key) -> bool {
  std::lock_guard<std::mutex> watcher_lock(app_state.gallery->folder_watchers_mutex);
  auto it = app_state.gallery->folder_watchers.find(watcher_key);
  if (it == app_state.gallery->folder_watchers.end()) {
    return false;
  }
  auto& watcher = it->second;

  // watcher 已进入停止流程时不能再恢复扫描。
  if (watcher.stop_requested.load(std::memory_order_acquire)) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(watcher.pending_mutex);
    // 只接受 Faulted -> Recovering，避免重复点击制造额外任务。
    if (watcher.sync_state != State::WatcherSyncState::Faulted) {
      return false;
    }

    watcher.sync_state = State::WatcherSyncState::Recovering;
    watcher.last_sync_error.reset();
    watcher.require_full_rescan = true;
    watcher.pending_file_changes.clear();
    watcher.pending_stable_file_changes.clear();
  }

  schedule_sync_task(app_state, watcher);
  return true;
}

// 读取当前语言文本；语言资源尚未就绪时使用调用方提供的默认文案。
auto get_i18n_text(Core::State::AppState& app_state, std::string_view key,
                   std::string_view fallback) -> std::string {
  if (app_state.i18n) {
    if (auto it = app_state.i18n->texts.find(std::string(key)); it != app_state.i18n->texts.end()) {
      return it->second;
    }
  }
  return std::string(fallback);
}

// 通知用户当前文件夹已暂停自动同步，并提供一次显式重试入口。
auto notify_sync_faulted(Core::State::AppState& app_state, State::FolderWatcherState& watcher,
                         const std::string& error) -> void {
  Core::Notifications::Types::NotificationOptions options;
  options.title =
      Utils::String::FromUtf8(get_i18n_text(app_state, "label.app_name", "SpinningMomo"));
  auto message = get_i18n_text(
      app_state, "message.gallery_folder_sync_failed",
      "Automatic gallery sync for this folder has been paused. Resolve the problem, then retry.");
  options.message = Utils::String::FromUtf8(
      std::format("{}\n{}\n{}", message, watcher.root_path.string(), error));
  options.duration = std::chrono::milliseconds(15000);

  // 通知可能晚于 watcher 生命周期，只保存稳定 key，并在点击时按 AppState 当前事实重查。
  auto watcher_key = watcher.root_path.string();
  options.action = Core::Notifications::Types::NotificationAction{
      .label =
          Utils::String::FromUtf8(get_i18n_text(app_state, "notification.action.retry", "Retry")),
      .callback =
          [watcher_key = std::move(watcher_key)](Core::State::AppState& callback_state) {
            retry_faulted_sync(callback_state, watcher_key);
          },
  };
  Core::Notifications::post_notification_request(app_state, std::move(options));
}

// 处理一次同步失败：增量失败升级全量，全量失败则暂停当前 root 等待用户介入。
auto handle_sync_failure(Core::State::AppState& app_state, State::FolderWatcherState& watcher,
                         bool attempted_full_rescan, const std::string& error) -> void {
  // 关闭导致的取消不是业务故障，不应触发恢复或用户通知。
  if (watcher.stop_requested.load(std::memory_order_acquire) ||
      app_state.gallery->scan_stop_source.stop_requested()) {
    return;
  }

  // 增量结果可能不完整，只自动升级一次全量对账。
  if (!attempted_full_rescan) {
    Logger().warn("Gallery incremental sync failed for '{}': {}. Falling back to full rescan.",
                  watcher.root_path.string(), error);
    prepare_full_recovery(watcher, error);
    schedule_sync_task(app_state, watcher);
    return;
  }

  // 全量仍失败时停止自动尝试，避免对同一外部问题持续重扫。
  Logger().error("Gallery full sync failed for '{}': {}. Automatic sync is now faulted.",
                 watcher.root_path.string(), error);
  mark_sync_faulted(watcher, error);
  notify_sync_faulted(app_state, watcher, error);
}

// 在已持有 pending_mutex 时判断是否仍有同步事实未消费。
auto has_pending_changes_locked(const State::FolderWatcherState& watcher) -> bool {
  return watcher.require_full_rescan || !watcher.pending_file_changes.empty() ||
         !watcher.pending_stable_file_changes.empty();
}

// 检查是否有待处理的变更（包含全量重扫标记或文件变更）
auto has_pending_changes(State::FolderWatcherState& watcher) -> bool {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  return has_pending_changes_locked(watcher);
}

// 获取并清空当前的待处理变更快照
auto take_pending_snapshot(State::FolderWatcherState& watcher) -> PendingSnapshot {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);

  PendingSnapshot snapshot;
  snapshot.require_full_rescan = watcher.require_full_rescan;
  snapshot.file_changes = std::move(watcher.pending_file_changes);

  watcher.require_full_rescan = false;
  watcher.sync_not_before.reset();

  return snapshot;
}

// 标记当前监听器需要执行全量重扫，同时清空原本的增量变更队列
auto mark_full_rescan(State::FolderWatcherState& watcher) -> void {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);
  // 文件夹有变化时，直接全量扫，逻辑最稳。
  watcher.require_full_rescan = true;
  watcher.pending_file_changes.clear();
  watcher.pending_stable_file_changes.clear();
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
auto enqueue_file_change(State::FolderWatcherState& watcher, const std::string& normalized_path,
                         State::PendingFileChangeAction action) -> void {
  std::lock_guard<std::mutex> lock(watcher.pending_mutex);

  // Faulted 期间继续吸收通知，但不再积累可能失真的逐文件动作。
  if (watcher.sync_state == State::WatcherSyncState::Faulted) {
    watcher.require_full_rescan = true;
    watcher.pending_file_changes.clear();
    watcher.pending_stable_file_changes.clear();
    return;
  }

  // 同一路径只留最后一次动作，避免重复处理。
  watcher.pending_file_changes[normalized_path] = action;
  watcher.pending_stable_file_changes.erase(normalized_path);
}

// 将 UPSERT 事件先放入稳定队列，避免录制中的文件被 watcher 当成成品反复分析。
auto enqueue_file_upsert_for_stability(State::FolderWatcherState& watcher,
                                       const std::string& normalized_path) -> void {
  auto now = std::chrono::steady_clock::now();
  auto probe_result = probe_file_state(std::filesystem::path(normalized_path));

  std::lock_guard<std::mutex> lock(watcher.pending_mutex);

  // Faulted 期间只记录后续恢复必须做全量对账。
  if (watcher.sync_state == State::WatcherSyncState::Faulted) {
    watcher.require_full_rescan = true;
    watcher.pending_file_changes.clear();
    watcher.pending_stable_file_changes.clear();
    return;
  }

  watcher.pending_file_changes.erase(normalized_path);

  auto& pending = watcher.pending_stable_file_changes[normalized_path];
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
}

// 二次探测到期候选：
// 只有当文件在一个静默窗口后，大小和修改时间都没有继续变化，才提升为真正的 UPSERT。
auto promote_stable_file_changes(State::FolderWatcherState& watcher) -> void {
  std::vector<std::pair<std::string, State::PendingStableFileChange>> due_candidates;
  auto now = std::chrono::steady_clock::now();

  {
    std::lock_guard<std::mutex> lock(watcher.pending_mutex);
    if (watcher.require_full_rescan) {
      return;
    }

    due_candidates.reserve(watcher.pending_stable_file_changes.size());
    for (const auto& [path, pending] : watcher.pending_stable_file_changes) {
      if (pending.ready_not_before <= now) {
        due_candidates.emplace_back(path, pending);
      }
    }
  }

  for (const auto& [path, pending] : due_candidates) {
    auto probe_result = probe_file_state(std::filesystem::path(path));

    std::lock_guard<std::mutex> lock(watcher.pending_mutex);
    auto it = watcher.pending_stable_file_changes.find(path);
    if (it == watcher.pending_stable_file_changes.end()) {
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
      watcher.pending_stable_file_changes.erase(it);
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

    watcher.pending_file_changes[path] = current.action;
    watcher.pending_stable_file_changes.erase(it);
  }
}

// 将接收到的待处理快照应用到数据库的最终逻辑（增量模式）
auto apply_incremental_sync(Core::State::AppState& app_state, State::FolderWatcherState& watcher,
                            const PendingSnapshot& snapshot)
    -> std::expected<Types::ScanResult, std::string> {
  Types::ScanResult result{};
  auto options = get_watcher_scan_options(watcher);
  auto stop_token = app_state.gallery->scan_stop_source.get_token();
  // 增量同步同样使用媒体和缩略图资源，cleanup 会等待这段共享区自然结束。
  std::shared_lock<std::shared_mutex> scan_lifetime_lock(app_state.gallery->scan_lifetime_mutex);
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
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
  auto ignore_rules = std::move(rules_result.value());
  const auto supported_extensions =
      options.supported_extensions.value_or(Scanner::Common::default_supported_extensions());

  std::vector<std::filesystem::path> upsert_paths;
  upsert_paths.reserve(snapshot.file_changes.size());

  // 先收集真正可能入库的 UPSERT 路径，避免被忽略或不支持的文件污染 folders 索引。
  for (const auto& [path, action] : snapshot.file_changes) {
    if (action != State::PendingFileChangeAction::UPSERT) {
      continue;
    }

    auto candidate_path = std::filesystem::path(path);
    if (!Scanner::Common::is_supported_file(candidate_path, supported_extensions)) {
      continue;
    }

    if (Features::Gallery::Ignore::Service::apply_ignore_rules(candidate_path, watcher.root_path,
                                                               ignore_rules, false)) {
      continue;
    }

    upsert_paths.emplace_back(std::move(candidate_path));
  }

  std::unordered_map<std::string, std::int64_t> folder_mapping;
  if (!upsert_paths.empty()) {
    auto folder_paths = Features::Gallery::Folder::Service::extract_unique_folder_paths(
        upsert_paths, watcher.root_path);
    auto mapping_result =
        Features::Gallery::Folder::Service::batch_create_folders_for_paths(app_state, folder_paths);
    if (mapping_result) {
      folder_mapping = std::move(mapping_result.value());
    } else {
      Logger().warn("Failed to pre-create folders for incremental sync '{}': {}",
                    watcher.root_path.string(), mapping_result.error());
    }
  }

  for (const auto& [path, action] : snapshot.file_changes) {
    if (stop_token.stop_requested()) {
      return std::unexpected("Gallery scan cancelled");
    }

    if (action != State::PendingFileChangeAction::REMOVE) {
      continue;
    }

    auto remove_result =
        Scanner::AssetPipeline::remove_asset_at_path(app_state, std::filesystem::path(path));
    if (!remove_result) {
      // 路径级失败：本轮尽量继续，不升级全量。
      result.errors.push_back(std::format("{}: {}", path, remove_result.error()));
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
    if (stop_token.stop_requested()) {
      return std::unexpected("Gallery scan cancelled");
    }

    if (action != State::PendingFileChangeAction::UPSERT) {
      continue;
    }

    auto upsert_result = Scanner::AssetPipeline::upsert_asset_at_path(
        app_state, watcher.root_path, options, ignore_rules, folder_mapping,
        std::filesystem::path(path), stop_token);
    if (!upsert_result) {
      // 路径级失败：解码/IO/单条写库问题只记入 errors，由后续事件或启动/全量兜底。
      result.errors.push_back(std::format("{}: {}", path, upsert_result.error()));
      continue;
    }

    using Scanner::AssetPipeline::PathSyncOutcome;
    switch (upsert_result.value()) {
      case PathSyncOutcome::Created:
        result.new_items++;
        // NEW / UPDATED 对派生消费者来说都属于“应确保目标状态存在”的 UPSERT。
        result.changes.push_back(Types::ScanChange{
            .path = path,
            .action = Types::ScanChangeAction::UPSERT,
        });
        break;
      case PathSyncOutcome::Updated:
        result.updated_items++;
        result.changes.push_back(Types::ScanChange{
            .path = path,
            .action = Types::ScanChangeAction::UPSERT,
        });
        break;
      case PathSyncOutcome::Removed:
        // UPSERT 路径上文件已消失：按删除统计，并向扩展发 REMOVE
        result.deleted_items++;
        result.changes.push_back(Types::ScanChange{
            .path = path,
            .action = Types::ScanChangeAction::REMOVE,
        });
        break;
      case PathSyncOutcome::Skipped:
      case PathSyncOutcome::UnchangedMeta:
        break;
    }
  }

  result.total_files = static_cast<int>(snapshot.file_changes.size());
  result.scan_duration = "incremental";
  return result;
}

// 执行全量重扫逻辑（直接代理到 Scanner 模块）
auto apply_full_rescan(Core::State::AppState& app_state, State::FolderWatcherState& watcher)
    -> std::expected<Types::ScanResult, std::string> {
  auto stop_token = app_state.gallery->scan_stop_source.get_token();
  // 一次全量同步全程持有共享锁，cleanup 会在释放媒体资源前等待它结束。
  std::shared_lock<std::shared_mutex> scan_lifetime_lock(app_state.gallery->scan_lifetime_mutex);
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  auto options = get_watcher_scan_options(watcher);
  auto scan_result = Features::Gallery::Scanner::scan_asset_directory(app_state, options);
  if (!scan_result) {
    return std::unexpected(scan_result.error());
  }

  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  auto thumbnail_repair_result = Features::Gallery::Asset::Thumbnail::repair_missing_thumbnails(
      app_state, watcher.root_path, options.thumbnail_short_edge.value_or(480));
  if (!thumbnail_repair_result) {
    Logger().warn("Gallery watcher thumbnail repair failed for '{}': {}",
                  watcher.root_path.string(), thumbnail_repair_result.error());
    return scan_result;
  }

  const auto& stats = thumbnail_repair_result.value();
  Logger().info(
      "Gallery watcher thumbnail repair finished for '{}'. context=full_rescan, candidates={}, "
      "missing={}, repaired={}, failed={}, skipped_missing_sources={}",
      watcher.root_path.string(), stats.candidate_hashes, stats.missing_thumbnails,
      stats.repaired_thumbnails, stats.failed_repairs, stats.skipped_missing_sources);
  return scan_result;
}

auto apply_offline_scan_changes(Core::State::AppState& app_state,
                                State::FolderWatcherState& watcher,
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

auto dispatch_scan_result(Core::State::AppState& app_state, State::FolderWatcherState& watcher,
                          const Types::ScanResult& result, std::string_view mode,
                          bool force_gallery_changed) -> void {
  // 统一收口：日志、gallery.changed 通知、post_scan_callback 都在这里发。
  // 这样启动恢复与运行时增量可以共用同一套“扫描完成后处理”。
  Logger().info(
      "Gallery sync finished for '{}'. mode={}, total={}, new={}, updated={}, deleted={}, "
      "errors={}",
      watcher.root_path.string(), mode, result.total_files, result.new_items, result.updated_items,
      result.deleted_items, result.errors.size());

  // 路径级 partial fail 不改变 Healthy/Faulted；汇总一条 warn 便于排查。
  if (!result.errors.empty()) {
    Logger().warn("Gallery sync path-level errors for '{}'. mode={}, count={}, first={}",
                  watcher.root_path.string(), mode, result.errors.size(), result.errors.front());
  }

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

// 标记监听器为全量重扫状态并触发下次同步任务。
auto request_full_rescan(Core::State::AppState& app_state, State::FolderWatcherState& watcher)
    -> void {
  mark_full_rescan(watcher);
  // Faulted 只更新 dirty 状态，必须由用户重试才能重新唤醒扫描。
  if (!is_sync_faulted(watcher)) {
    schedule_sync_task(app_state, watcher);
  }
}

// 启动阶段串行执行一次全量对账，并将失败收敛为当前 root 的 Faulted。
auto run_startup_full_rescan(Core::State::AppState& app_state, State::FolderWatcherState& watcher)
    -> std::expected<void, std::string> {
  // 调用方持有 sync_execution_mutex，启动恢复与运行时同步不会并行。
  // 启动恢复直接消费全量标记，避免全局编排线程重复执行同一轮扫描。
  mark_full_rescan(watcher);
  [[maybe_unused]] auto startup_snapshot = take_pending_snapshot(watcher);

  // 启动阶段必须当场完成，所有 root 收敛后才能进行全局缩略图缓存对账。
  auto sync_result = apply_full_rescan(app_state, watcher);
  if (sync_result) {
    mark_sync_healthy(watcher);
    dispatch_scan_result(app_state, watcher, sync_result.value(), "startup_full", true);
  } else {
    handle_sync_failure(app_state, watcher, true, sync_result.error());
  }

  // 恢复完成后唤醒启动扫描期间新到达的文件事件。
  if (!watcher.stop_requested.load(std::memory_order_acquire) && !is_sync_faulted(watcher) &&
      has_pending_changes(watcher)) {
    schedule_sync_task(app_state, watcher);
  }

  if (!sync_result) {
    return std::unexpected(sync_result.error());
  }

  return {};
}

struct ScheduledWatcher {
  std::string key;
  std::chrono::steady_clock::time_point due_at;
};

// 从所有 root 中选出最早到期的同步任务，只返回稳定 key 而不携带状态指针。
auto find_next_scheduled_watcher(Core::State::AppState& app_state)
    -> std::optional<ScheduledWatcher> {
  std::optional<ScheduledWatcher> next;
  const auto now = std::chrono::steady_clock::now();

  std::lock_guard<std::mutex> watcher_lock(app_state.gallery->folder_watchers_mutex);
  for (auto& [key, watcher] : app_state.gallery->folder_watchers) {
    if (watcher.removal_in_progress.load(std::memory_order_acquire) ||
        watcher.stop_requested.load(std::memory_order_acquire)) {
      continue;
    }

    std::lock_guard<std::mutex> pending_lock(watcher.pending_mutex);
    // 启动恢复先建立基线；Faulted root 等用户明确重试。
    if (watcher.startup_recovery_in_progress ||
        watcher.sync_state == State::WatcherSyncState::Faulted) {
      continue;
    }

    std::optional<std::chrono::steady_clock::time_point> due_at;
    if (watcher.require_full_rescan || !watcher.pending_file_changes.empty()) {
      due_at = watcher.sync_not_before.value_or(now);
    } else {
      for (const auto& [_, pending] : watcher.pending_stable_file_changes) {
        due_at = due_at.has_value() ? std::min(due_at.value(), pending.ready_not_before)
                                    : pending.ready_not_before;
      }
    }

    if (due_at.has_value() && (!next.has_value() || due_at.value() < next->due_at)) {
      next = ScheduledWatcher{.key = key, .due_at = due_at.value()};
    }
  }
  return next;
}

// 按 key 重新定位 root，并在生命周期保护下执行一轮同步。
auto process_scheduled_watcher(Core::State::AppState& app_state, const std::string& watcher_key,
                               std::stop_token stop_token) -> void {
  State::FolderWatcherState* watcher_ptr = nullptr;
  std::unique_lock<std::mutex> lifecycle_lock;
  {
    std::lock_guard<std::mutex> watcher_lock(app_state.gallery->folder_watchers_mutex);
    auto it = app_state.gallery->folder_watchers.find(watcher_key);
    if (it == app_state.gallery->folder_watchers.end() ||
        it->second.removal_in_progress.load(std::memory_order_acquire)) {
      return;
    }
    watcher_ptr = &it->second;
    // 从注册表锁桥接到 root 生命周期锁，防止取得执行锁前状态被删除。
    lifecycle_lock = std::unique_lock<std::mutex>(watcher_ptr->watch_lifecycle_mutex);
  }
  auto& watcher = *watcher_ptr;

  // 与启动恢复共享执行锁，保证同一 root 同时只有一条扫描链。
  std::unique_lock<std::mutex> execution_lock(watcher.sync_execution_mutex);
  lifecycle_lock.unlock();
  if (stop_token.stop_requested() || watcher.stop_requested.load(std::memory_order_acquire)) {
    return;
  }

  bool attempted_full_rescan = false;
  try {
    // 先把已经安静下来的候选文件提升到最终增量队列。
    promote_stable_file_changes(watcher);
    auto snapshot = take_pending_snapshot(watcher);
    if (!snapshot.require_full_rescan && snapshot.file_changes.empty()) {
      return;
    }

    // 全量标记覆盖逐文件动作；失败策略也以本轮实际模式为准。
    attempted_full_rescan = snapshot.require_full_rescan;
    auto sync_result = snapshot.require_full_rescan
                           ? apply_full_rescan(app_state, watcher)
                           : apply_incremental_sync(app_state, watcher, snapshot);
    if (!sync_result) {
      handle_sync_failure(app_state, watcher, attempted_full_rescan, sync_result.error());
      return;
    }

    // 磁盘对账成功即恢复 Healthy；分发异常不反向污染同步状态。
    mark_sync_healthy(watcher);
    try {
      dispatch_scan_result(app_state, watcher, sync_result.value(),
                           snapshot.require_full_rescan ? "full" : "incremental",
                           snapshot.require_full_rescan);
    } catch (const std::exception& e) {
      Logger().error("Gallery sync result dispatch failed for '{}': {}", watcher.root_path.string(),
                     e.what());
    } catch (...) {
      Logger().error("Gallery sync result dispatch failed for '{}' with unknown error",
                     watcher.root_path.string());
    }
  } catch (const std::exception& e) {
    handle_sync_failure(app_state, watcher, attempted_full_rescan, e.what());
  } catch (...) {
    handle_sync_failure(app_state, watcher, attempted_full_rescan, "Unknown gallery sync failure");
  }
}

// Gallery 全局同步编排循环：选择最早到期 root，无任务时事件驱动休眠。
auto run_sync_coordinator(Core::State::AppState& app_state, std::stop_token stop_token) -> void {
  while (!stop_token.stop_requested()) {
    std::uint64_t observed_generation = 0;
    {
      std::lock_guard<std::mutex> lock(app_state.gallery->watcher_sync_mutex);
      observed_generation = app_state.gallery->watcher_sync_generation;
    }

    auto next = find_next_scheduled_watcher(app_state);
    const auto now = std::chrono::steady_clock::now();
    if (next.has_value() && next->due_at <= now) {
      process_scheduled_watcher(app_state, next->key, stop_token);
      continue;
    }

    std::unique_lock<std::mutex> lock(app_state.gallery->watcher_sync_mutex);
    auto schedule_changed = [&] {
      return app_state.gallery->watcher_sync_generation != observed_generation;
    };
    if (next.has_value()) {
      // 新事件会提前唤醒，重新比较所有 root 的最早期限。
      app_state.gallery->watcher_sync_condition.wait_until(lock, stop_token, next->due_at,
                                                           schedule_changed);
    } else {
      app_state.gallery->watcher_sync_condition.wait(lock, stop_token, schedule_changed);
    }
  }
}

// pending 状态是任务事实；调度只更新防抖期限并唤醒全局编排线程。
auto schedule_sync_task(Core::State::AppState& app_state, State::FolderWatcherState& watcher)
    -> void {
  {
    std::lock_guard<std::mutex> lock(watcher.pending_mutex);
    if (watcher.require_full_rescan || !watcher.pending_file_changes.empty()) {
      // 可执行变更的静默窗口从最新一批事件重新计时。
      watcher.sync_not_before = std::chrono::steady_clock::now() + kDebounceDelay;
    }
  }
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->watcher_sync_mutex);
    ++app_state.gallery->watcher_sync_generation;
  }
  app_state.gallery->watcher_sync_condition.notify_one();
}

}  // namespace Features::Gallery::Watcher::Sync
