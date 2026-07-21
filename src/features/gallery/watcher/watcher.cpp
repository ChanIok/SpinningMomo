module;

module Features.Gallery.Watcher;

import std;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Recovery.Service;
import Features.Gallery.Watcher.Sync;
import Features.Gallery.Watcher.Notify;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.RootAvailability;
import Utils.Logger;
import Utils.Path;
import <windows.h>;
import <wil/resource.h>;

namespace Features::Gallery::Watcher {

// 手动 move 结束后额外缓冲一段时间，吸收“晚到”的文件系统通知。
constexpr std::chrono::milliseconds kManualMoveIgnoreGracePeriod{3000};

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

// 保存已经成功应用的启动恢复边界，作为下次启动重放 USN 的保守起点。
auto persist_startup_recovery_plan(
    Core::State::AppState& app_state,
    const Features::Gallery::Recovery::Types::StartupRecoveryPlan& plan) -> void {
  // 非 NTFS/无 Journal 的计划没有可恢复边界，继续依赖下次全量扫描。
  if (plan.root_path.empty() || plan.volume_identity.empty() || plan.rule_fingerprint.empty() ||
      !plan.journal_id.has_value() || !plan.checkpoint_usn.has_value()) {
    return;
  }

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
    Logger().warn("Failed to persist startup recovery checkpoint for '{}': {}", plan.root_path,
                  persist_result.error());
  }
}

// 启动 Gallery 唯一的同步编排线程。
auto start_sync_coordinator_if_needed(Core::State::AppState& app_state)
    -> std::expected<bool, std::string> {
  if (is_shutdown_requested(app_state)) {
    return std::unexpected("Gallery is shutting down");
  }

  std::lock_guard<std::mutex> lock(app_state.gallery->watcher_sync_mutex);
  // 等锁期间可能已进入 shutdown，锁内重查阻止线程被重新创建。
  if (is_shutdown_requested(app_state)) {
    return std::unexpected("Gallery is shutting down");
  }
  if (app_state.gallery->watcher_sync_thread.joinable()) {
    return false;
  }

  try {
    // 编排线程不占用 WorkerPool，扫描内部的叶子任务仍由 WorkerPool 处理。
    app_state.gallery->watcher_sync_thread = std::jthread([&app_state](std::stop_token stop_token) {
      Sync::run_sync_coordinator(app_state, stop_token);
    });
  } catch (const std::exception& e) {
    return std::unexpected("Failed to start gallery sync coordinator: " + std::string(e.what()));
  }
  return true;
}

// 停止 Gallery 全局同步编排线程，并打断无任务时的条件等待。
auto stop_sync_coordinator(Core::State::AppState& app_state) -> void {
  std::jthread coordinator_thread;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->watcher_sync_mutex);
    if (!app_state.gallery->watcher_sync_thread.joinable()) {
      return;
    }
    // 锁内移出线程所有权，后续 join 不再与启动入口并发读写同一 jthread。
    app_state.gallery->watcher_sync_thread.request_stop();
    coordinator_thread = std::move(app_state.gallery->watcher_sync_thread);
  }

  app_state.gallery->watcher_sync_condition.notify_all();
  coordinator_thread.join();
}

// 在已独占监听生命周期锁时停止该 root 的目录读取线程。
auto stop_watch_thread(State::FolderWatcherState& watcher) -> void {
  // 先关闭该 root 的事件接收与同步入口。
  watcher.stop_requested.store(true, std::memory_order_release);

  // 取消阻塞中的目录读取，让监听线程无需等待下一条文件事件即可退出。
  if (watcher.watch_thread.joinable()) {
    watcher.watch_thread.request_stop();

    auto raw_handle = watcher.directory_handle.load(std::memory_order_acquire);
    auto* directory_handle = static_cast<HANDLE>(raw_handle);
    if (directory_handle && directory_handle != INVALID_HANDLE_VALUE) {
      CancelIoEx(directory_handle, nullptr);
    }

    // join 后该 root 不会再产生新的待处理事实。
    watcher.watch_thread.join();
  }
}

// 停止指定 watcher：关闭监听后等待该 root 当前同步退出。
auto stop_watcher(State::FolderWatcherState& watcher) -> void {
  {
    std::unique_lock<std::mutex> lifecycle_lock(watcher.watch_lifecycle_mutex);
    stop_watch_thread(watcher);
  }
  // 启动恢复和全局编排线程都共享此锁，离开后状态才可销毁。
  std::lock_guard<std::mutex> execution_lock(watcher.sync_execution_mutex);
}

// 在调用方持有监听生命周期锁时启动该 root 的目录读取线程。
auto start_watch_thread_if_needed(Core::State::AppState& app_state, const std::string& watcher_key,
                                  State::FolderWatcherState& watcher, bool bootstrap_full_scan)
    -> std::expected<bool, std::string> {
  if (watcher.removal_in_progress.load(std::memory_order_acquire)) {
    return std::unexpected("Watcher is stopping for root directory: " + watcher_key);
  }
  if (watcher.watch_thread.joinable()) {
    // 同一 root 只能拥有一条目录监听线程。
    return false;
  }

  watcher.stop_requested.store(false, std::memory_order_release);

  try {
    // 全局编排器已由上层先行启动，此处只产生目录事件。
    watcher.watch_thread = std::jthread([&app_state, watcher_key](std::stop_token stop_token) {
      Notify::run_watch_loop(app_state, watcher_key, stop_token);
    });
  } catch (const std::exception& e) {
    stop_watch_thread(watcher);
    return std::unexpected("Failed to start watcher thread: " + std::string(e.what()));
  }

  if (bootstrap_full_scan) {
    // 首次启动通过同一调度入口请求全量对账，不额外创建扫描任务。
    Sync::request_full_rescan(app_state, watcher);
  }

  return true;
}

// Gallery root 的运行时 key 只做 lexical 规范化，不解析 symlink/网络路径。
auto normalize_root_directory(const std::filesystem::path& root_directory)
    -> std::expected<std::filesystem::path, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(root_directory);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize root directory: " + normalized_result.error());
  }

  return normalized_result.value();
}

auto validate_root_directory_accessible(const std::filesystem::path& root_directory)
    -> std::expected<void, std::string> {
  if (!std::filesystem::exists(root_directory)) {
    return std::unexpected("Root directory does not exist: " + root_directory.string());
  }
  if (!std::filesystem::is_directory(root_directory)) {
    return std::unexpected("Root path is not a directory: " + root_directory.string());
  }

  return {};
}

// 注册一个根目录 watcher；若已存在则更新扫描参数。
auto register_watcher_for_directory(Core::State::AppState& app_state,
                                    const std::filesystem::path& root_directory,
                                    const std::optional<Types::ScanOptions>& scan_options)
    -> std::expected<void, std::string> {
  // shutdown 开始后不再接受新 watcher，避免清空 watcher 表后又被并发写回。
  if (is_shutdown_requested(app_state)) {
    return std::unexpected("Gallery shutdown has been requested");
  }

  auto normalized_result = normalize_root_directory(root_directory);
  if (!normalized_result) {
    return std::unexpected(normalized_result.error());
  }
  auto normalized_root = normalized_result.value();
  auto key = normalized_root.string();

  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    // 与 shutdown_watchers 使用同一把锁，再检查一次以关闭检查与插入之间的竞态窗口。
    if (is_shutdown_requested(app_state)) {
      return std::unexpected("Gallery shutdown has been requested");
    }

    // FolderWatcherState 含 mutex，必须在节点容器中原地构造，不能先创建再移动。
    auto [it, watcher_created] = app_state.gallery->folder_watchers.try_emplace(key);
    auto& watcher = it->second;
    if (watcher.removal_in_progress.load(std::memory_order_acquire)) {
      return std::unexpected("Watcher is stopping for root directory: " + key);
    }
    if (watcher_created) {
      watcher.root_path = normalized_root;
    }

    if (watcher_created || scan_options.has_value()) {
      Sync::update_watcher_scan_options(watcher, scan_options);
    }
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

  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    auto it = app_state.gallery->folder_watchers.find(normalized_result->string());
    if (it == app_state.gallery->folder_watchers.end()) {
      return std::unexpected("Watcher is not registered for root directory: " +
                             normalized_result->string());
    }
    if (it->second.removal_in_progress.load(std::memory_order_acquire)) {
      return std::unexpected("Watcher is stopping for root directory: " +
                             normalized_result->string());
    }
    Sync::update_post_scan_callback(it->second, std::move(post_scan_callback));
  }
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

  if (Features::Gallery::RootAvailability::is_remote_unreachable(app_state,
                                                                 normalized_result.value())) {
    return std::unexpected("Remote root is unavailable: " + normalized_result->string());
  }

  State::FolderWatcherState* watcher = nullptr;
  std::unique_lock<std::mutex> lifecycle_lock;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    auto it = app_state.gallery->folder_watchers.find(normalized_result->string());
    if (it == app_state.gallery->folder_watchers.end()) {
      return std::unexpected("Watcher is not registered for root directory: " +
                             normalized_result->string());
    }
    watcher = &it->second;
    if (watcher->removal_in_progress.load(std::memory_order_acquire)) {
      return std::unexpected("Watcher is stopping for root directory: " +
                             normalized_result->string());
    }
    // 监听生命周期锁跨过查找与启动，删除只能在本次更新结束后继续。
    lifecycle_lock = std::unique_lock<std::mutex>(watcher->watch_lifecycle_mutex);
  }

  // 消费者必须先于目录事件生产者存在。
  auto coordinator_result = start_sync_coordinator_if_needed(app_state);
  if (!coordinator_result) {
    return std::unexpected(coordinator_result.error());
  }

  auto start_result = start_watch_thread_if_needed(app_state, normalized_result->string(), *watcher,
                                                   bootstrap_full_scan);
  if (!start_result) {
    return std::unexpected(start_result.error());
  }

  return {};
}

// 从程序缓存中移除对应目录的监听器并停止后台线程
auto remove_watcher_for_directory(Core::State::AppState& app_state,
                                  const std::filesystem::path& root_directory)
    -> std::expected<bool, std::string> {
  auto normalized_result = normalize_root_directory(root_directory);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize root directory: " + normalized_result.error());
  }

  auto key = normalized_result->string();
  State::FolderWatcherState* watcher = nullptr;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    auto it = app_state.gallery->folder_watchers.find(key);
    if (it == app_state.gallery->folder_watchers.end() ||
        it->second.removal_in_progress.load(std::memory_order_acquire)) {
      return false;
    }
    // 先在注册表中标记停止，阻止同 key 被重新配置或启动；对象保留到线程全部退出。
    it->second.removal_in_progress.store(true, std::memory_order_release);
    it->second.stop_requested.store(true, std::memory_order_release);
    watcher = &it->second;
  }

  stop_watcher(*watcher);

  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    auto it = app_state.gallery->folder_watchers.find(key);
    if (it != app_state.gallery->folder_watchers.end() && &it->second == watcher) {
      app_state.gallery->folder_watchers.erase(it);
    }
  }
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

  std::vector<std::string> watcher_keys;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    watcher_keys.reserve(app_state.gallery->folder_watchers.size());
    for (const auto& [key, _] : app_state.gallery->folder_watchers) {
      watcher_keys.push_back(key);
    }
  }

  if (!watcher_keys.empty()) {
    // 先建立唯一消费者，再逐个启动 root 监听线程。
    auto coordinator_result = start_sync_coordinator_if_needed(app_state);
    if (!coordinator_result) {
      return std::unexpected(coordinator_result.error());
    }
  }

  size_t started_count = 0;
  std::optional<std::string> first_error;
  std::uint32_t startup_thumbnail_short_edge = 480;

  // 公共 helper：启动 watcher 线程并统一处理计数和错误记录。
  auto try_start_watcher = [&](const std::string& key, State::FolderWatcherState& watcher) -> bool {
    if (is_shutdown_requested(app_state)) {
      Logger().info("Skip watcher start for '{}': shutdown has been requested",
                    watcher.root_path.string());
      return false;
    }

    auto validate_result = validate_root_directory_accessible(watcher.root_path);
    if (!validate_result) {
      Logger().warn("Skip watcher start for unreachable root '{}': {}", watcher.root_path.string(),
                    validate_result.error());
      if (!first_error.has_value()) {
        first_error = validate_result.error();
      }
      return false;
    }

    auto result = start_watch_thread_if_needed(app_state, key, watcher, false);
    if (!result) {
      Logger().warn("Skip watcher start for '{}': {}", watcher.root_path.string(), result.error());
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

  for (const auto& watcher_key : watcher_keys) {
    if (is_shutdown_requested(app_state)) {
      Logger().info("Stop gallery watcher startup recovery: shutdown has been requested");
      break;
    }

    State::FolderWatcherState* watcher_ptr = nullptr;
    std::unique_lock<std::mutex> lifecycle_lock;
    {
      std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
      auto it = app_state.gallery->folder_watchers.find(watcher_key);
      if (it == app_state.gallery->folder_watchers.end() ||
          it->second.removal_in_progress.load(std::memory_order_acquire)) {
        continue;
      }
      watcher_ptr = &it->second;
      // 监听生命周期锁跨过查找与线程启动，删除无法在此期间销毁状态。
      lifecycle_lock = std::unique_lock<std::mutex>(watcher_ptr->watch_lifecycle_mutex);
    }
    auto& watcher = *watcher_ptr;

    startup_thumbnail_short_edge =
        std::max(startup_thumbnail_short_edge,
                 Sync::get_watcher_scan_options(watcher).thumbnail_short_edge.value_or(480));

    if (Features::Gallery::RootAvailability::is_remote_unreachable(app_state, watcher.root_path)) {
      Logger().warn("Skip gallery startup recovery for unavailable remote root '{}'",
                    watcher.root_path.string());
      continue;
    }

    // 注册表锁已经释放，再取得长操作执行锁，避免与扫描回调形成反向锁序。
    std::unique_lock<std::mutex> execution_lock(watcher.sync_execution_mutex);

    // 先暂停实时队列消费并启动监听，让恢复期间的新事件只入队、不抢跑数据库同步。
    Sync::begin_startup_recovery(watcher);
    [[maybe_unused]] auto recovery_gate = wil::scope_exit(
        [&app_state, &watcher] { Sync::finish_startup_recovery(app_state, watcher); });
    if (!try_start_watcher(watcher_key, watcher)) {
      continue;
    }
    lifecycle_lock.unlock();

    // 实时监听建立后再划定 USN 边界：离线计划负责边界之前，pending 队列负责边界之后。
    auto recovery_plan_result = Features::Gallery::Recovery::Service::prepare_startup_recovery(
        app_state, watcher.root_path, Sync::get_watcher_scan_options(watcher));
    if (!recovery_plan_result) {
      Logger().warn("Startup recovery decision failed for '{}': {}. Falling back to full scan.",
                    watcher.root_path.string(), recovery_plan_result.error());
      auto startup_full_scan_result = Sync::run_startup_full_rescan(app_state, watcher);
      if (!startup_full_scan_result) {
        Logger().warn("Startup full scan failed for '{}': {}", watcher.root_path.string(),
                      startup_full_scan_result.error());
      }
      continue;
    }

    const auto& plan = recovery_plan_result.value();

    if (plan.mode == Features::Gallery::Recovery::Types::StartupRecoveryMode::UsnJournal) {
      Logger().info("Gallery startup recovery for '{}': mode=usn, reason={}, changes={}",
                    watcher.root_path.string(), plan.reason, plan.changes.size());

      if (!plan.changes.empty()) {
        auto recovery_apply_result =
            Sync::apply_offline_scan_changes(app_state, watcher, plan.changes);
        if (!recovery_apply_result) {
          Logger().warn("USN recovery apply failed for '{}': {}. Falling back to full scan.",
                        watcher.root_path.string(), recovery_apply_result.error());
          auto startup_full_scan_result = Sync::run_startup_full_rescan(app_state, watcher);
          if (!startup_full_scan_result) {
            Logger().warn("Startup full scan failed for '{}': {}", watcher.root_path.string(),
                          startup_full_scan_result.error());
          } else {
            // 全量已覆盖计划边界；之后的事件即使本次实时处理，下次启动仍允许安全重放。
            persist_startup_recovery_plan(app_state, plan);
          }
          continue;
        }
        Sync::dispatch_scan_result(app_state, watcher, recovery_apply_result.value(),
                                   "startup_usn");
      }

      // 只推进到已成功应用的计划边界，不把恢复期间的实时事件误标为已处理。
      persist_startup_recovery_plan(app_state, plan);
      continue;
    }

    Logger().info("Gallery startup recovery for '{}': mode=full_scan, reason={}",
                  watcher.root_path.string(), plan.reason);

    auto startup_full_scan_result = Sync::run_startup_full_rescan(app_state, watcher);
    if (!startup_full_scan_result) {
      Logger().warn("Startup full scan failed for '{}': {}", watcher.root_path.string(),
                    startup_full_scan_result.error());
    } else {
      // 全量扫描建立了当前基线，保存扫描开始前取得的边界，后续变化仍可安全重放。
      persist_startup_recovery_plan(app_state, plan);
    }
  }

  // 退出阶段不再启动全局缩略图对账，让启动恢复任务尽快结束。
  if (is_shutdown_requested(app_state)) {
    Logger().info("Stop gallery watcher startup recovery before thumbnail reconcile");
    return {};
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

  Logger().info("Gallery watchers started: {} / {}", started_count, watcher_keys.size());
  if (first_error.has_value()) {
    return std::unexpected(first_error.value());
  }
  return {};
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

auto dispatch_manual_scan_changes(Core::State::AppState& app_state,
                                  const std::vector<Types::ScanChange>& changes)
    -> std::expected<void, std::string> {
  if (changes.empty()) {
    return {};
  }

  struct ManualDispatchTarget {
    std::filesystem::path root_path;
    std::function<void(const Types::ScanResult&)> post_scan_callback;
  };
  std::vector<ManualDispatchTarget> targets;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    targets.reserve(app_state.gallery->folder_watchers.size());
    for (auto& [_, watcher] : app_state.gallery->folder_watchers) {
      if (watcher.removal_in_progress.load(std::memory_order_acquire)) {
        continue;
      }
      // 锁内只复制分发所需事实，回调在锁外执行，避免携带 watcher 生命周期。
      std::lock_guard<std::mutex> pending_lock(watcher.pending_mutex);
      targets.push_back(ManualDispatchTarget{
          .root_path = watcher.root_path,
          .post_scan_callback = watcher.post_scan_callback,
      });
    }
  }

  if (targets.empty()) {
    return {};
  }

  Logger().info("Gallery manual scan change dispatch: incoming changes={}, watchers={}",
                changes.size(), targets.size());

  std::unordered_map<std::string, std::vector<Types::ScanChange>> changes_by_watcher_root;
  changes_by_watcher_root.reserve(targets.size());

  for (const auto& change : changes) {
    auto changed_path_result = Utils::Path::NormalizePath(std::filesystem::path(change.path));
    if (!changed_path_result) {
      return std::unexpected("Failed to normalize manual scan change path '" + change.path +
                             "': " + changed_path_result.error());
    }

    bool matched = false;
    for (const auto& target : targets) {
      if (!Utils::Path::IsPathWithinBase(changed_path_result.value(), target.root_path)) {
        continue;
      }

      auto& bucket = changes_by_watcher_root[target.root_path.string()];
      bucket.push_back(Types::ScanChange{
          .path = changed_path_result->string(),
          .action = change.action,
      });
      matched = true;
    }

    if (!matched) {
      Logger().debug("Skip manual scan change dispatch: '{}' does not belong to any watcher root",
                     changed_path_result->string());
    }
  }

  for (const auto& target : targets) {
    auto bucket_it = changes_by_watcher_root.find(target.root_path.string());
    if (bucket_it == changes_by_watcher_root.end() || bucket_it->second.empty()) {
      continue;
    }

    Types::ScanResult result;
    result.total_files = static_cast<int>(bucket_it->second.size());
    result.scan_duration = "manual_dispatch";
    result.changes = std::move(bucket_it->second);

    for (const auto& change : result.changes) {
      if (change.action == Types::ScanChangeAction::REMOVE) {
        result.deleted_items++;
      } else {
        // 手动上报场景中，UPSERT 语义是“确保目标状态存在”，统一计入 updated。
        result.updated_items++;
      }
    }

    if (target.post_scan_callback) {
      Logger().info(
          "Gallery manual scan change dispatch: root='{}', changes={}, updated={}, deleted={}",
          target.root_path.string(), result.changes.size(), result.updated_items,
          result.deleted_items);
      target.post_scan_callback(result);
    }
  }

  return {};
}

// 在应用关闭时，优雅关闭所有的监听器线程和句柄资源
auto shutdown_watchers(Core::State::AppState& app_state) -> void {
  std::vector<State::FolderWatcherState*> watchers;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    for (auto& [_, watcher] : app_state.gallery->folder_watchers) {
      watcher.removal_in_progress.store(true, std::memory_order_release);
      watcher.stop_requested.store(true, std::memory_order_release);
      watchers.push_back(&watcher);
    }
  }

  for (auto* watcher : watchers) {
    // 先停各 root 监听并等待已在执行的同步，避免退出阶段产生新任务。
    stop_watcher(*watcher);
  }

  // 所有 root 都已禁止调度，此时停止唯一的全局消费者。
  stop_sync_coordinator(app_state);

  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    app_state.gallery->folder_watchers.clear();
  }

  Logger().info("Gallery watchers stopped: {}", watchers.size());
}

}  // namespace Features::Gallery::Watcher
