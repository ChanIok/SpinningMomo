module;

module Extensions.InfinityNikki.PhotoService;

import std;
import Core.State;
import Core.Tasks;
import Core.WorkerPool;
import Features.Gallery;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Repository;
import Features.Gallery.Watcher;
import Features.Gallery.Types;
import Features.Settings.State;
import Extensions.InfinityNikki.TaskService;
import Extensions.InfinityNikki.ScreenshotHardlinks;
import Extensions.InfinityNikki.Types;
import Utils.Logger;
import Utils.Path;

namespace Extensions::InfinityNikki::PhotoService {

struct ServiceState {
  std::mutex mutex;
  std::filesystem::path current_watch_path;
};

auto service_state() -> ServiceState& {
  static ServiceState instance;
  return instance;
}

// 生成《无限暖暖》照片专用的忽略推断规则
auto make_infinity_nikki_ignore_rules() -> std::vector<Features::Gallery::Types::ScanIgnoreRule> {
  using Features::Gallery::Types::ScanIgnoreRule;

  return {
      ScanIgnoreRule{.pattern = "^.*$", .pattern_type = "regex", .rule_type = "exclude"},
      ScanIgnoreRule{.pattern = "^[0-9]+/NikkiPhotos_HighQuality(/.*)?$",
                     .pattern_type = "regex",
                     .rule_type = "include"},
      ScanIgnoreRule{.pattern = "^[0-9]+/Videos/[^/]+/[^/]+\\.mp4$",
                     .pattern_type = "regex",
                     .rule_type = "include"},
  };
}

// 确保监听根目录的过滤规则在数据库中存在并生效
auto ensure_watch_root_ignore_rules(Core::State::AppState& app_state,
                                    const std::filesystem::path& watch_root)
    -> std::expected<void, std::string> {
  auto normalized_watch_root_result = Utils::Path::NormalizePath(watch_root);
  if (!normalized_watch_root_result) {
    return std::unexpected("Failed to normalize GamePlayPhotos root folder: " +
                           normalized_watch_root_result.error());
  }

  auto normalized_watch_root = normalized_watch_root_result.value();
  std::vector<std::filesystem::path> root_paths = {normalized_watch_root};
  auto folder_mapping_result =
      Features::Gallery::Folder::Service::batch_create_folders_for_paths(app_state, root_paths);
  if (!folder_mapping_result) {
    return std::unexpected("Failed to create GamePlayPhotos root folder: " +
                           folder_mapping_result.error());
  }

  auto root_key = normalized_watch_root.string();
  auto root_it = folder_mapping_result->find(root_key);
  if (root_it == folder_mapping_result->end()) {
    return std::unexpected("Failed to resolve GamePlayPhotos root folder id");
  }

  auto persist_result = Features::Gallery::Ignore::Repository::replace_rules_by_folder_id(
      app_state, root_it->second, make_infinity_nikki_ignore_rules());
  if (!persist_result) {
    return std::unexpected("Failed to persist InfinityNikki ignore rules: " +
                           persist_result.error());
  }

  return {};
}

// 每次画廊扫描完毕后触发的回调处理，包含同步 ScreenShot 硬链接和提取照片参数
auto on_gallery_scan_complete(Core::State::AppState& app_state,
                              const Features::Gallery::Types::ScanResult& result) -> void {
  if (!app_state.settings) {
    return;
  }

  const auto& config = app_state.settings->raw.extensions.infinity_nikki;

  if (config.manage_screenshot_hardlinks) {
    if (Core::Tasks::has_active_task_of_type(
            app_state, "extensions.infinityNikki.initializeScreenshotHardlinks")) {
      Logger().debug("Skip InfinityNikki screenshot hardlink sync: initialization task is active");
    } else {
      // 增量 watcher 会携带逐文件 changes；只有拿不到变化集时才回退到全量 sync。
      auto runtime_changes = result.changes;
      bool submitted = Core::WorkerPool::submit_task(
          *app_state.worker_pool, [&app_state, runtime_changes = std::move(runtime_changes)]() {
            auto sync_result =
                runtime_changes.empty()
                    ? Extensions::InfinityNikki::ScreenshotHardlinks::sync(app_state)
                    : Extensions::InfinityNikki::ScreenshotHardlinks::apply_runtime_changes(
                          app_state, runtime_changes);
            if (!sync_result) {
              Logger().warn("InfinityNikki screenshot hardlinks sync failed: {}",
                            sync_result.error());
            } else {
              const auto& r = sync_result.value();
              Logger().info(
                  "InfinityNikki screenshot hardlinks synced: source={}, created={}, updated={}, "
                  "removed={}, ignored={}",
                  r.source_count, r.created_count, r.updated_count, r.removed_count,
                  r.ignored_count);
            }
          });
      if (!submitted) {
        Logger().warn("InfinityNikki hardlinks sync: failed to submit worker task");
      }
    }
  }

  if (config.allow_online_photo_metadata_extract) {
    // 静默自动解析只消费“本次扫描变更”里的 UPSERT 资产；
    // 不再从全库补齐 missing，避免拍 1 张触发历史批量解析。
    // 注意：全量 scan 的 ScanResult.changes 允许为空，因此这里不保证能覆盖“首次全量导入”。
    std::vector<std::int64_t> candidate_asset_ids;
    candidate_asset_ids.reserve(result.changes.size());
    std::unordered_set<std::int64_t> seen_ids;
    seen_ids.reserve(result.changes.size());

    for (const auto& change : result.changes) {
      if (change.action != Features::Gallery::Types::ScanChangeAction::UPSERT) {
        continue;
      }

      auto asset_result =
          Features::Gallery::Asset::Repository::get_asset_by_path(app_state, change.path);
      if (!asset_result) {
        Logger().warn("Skip silent extract candidate '{}': {}", change.path, asset_result.error());
        continue;
      }
      if (!asset_result->has_value()) {
        // watcher 与 DB 之间可能有短暂时序差；拿不到资产就跳过本条。
        continue;
      }

      auto asset_id = asset_result->value().id;
      if (seen_ids.insert(asset_id).second) {
        candidate_asset_ids.push_back(asset_id);
      }
    }

    if (!candidate_asset_ids.empty()) {
      // 传递资产 ID 而非数量/时间窗口，保证候选集与本次变更一一对应。
      Extensions::InfinityNikki::TaskService::schedule_silent_extract_photo_params(
          app_state, InfinityNikkiSilentExtractPhotoParamsRequest{
                         .candidate_asset_ids = std::move(candidate_asset_ids),
                     });
    }
  }
}

// 首次 initial scan 完成后的补偿路径：
// initial scan 关注最终一致性，changes 可能为空，导致 on_gallery_scan_complete 里的增量提取无候选。
// 因此这里显式触发一次全量解析任务（only_missing=false），确保首次导入可直接获得元数据。
auto maybe_start_full_extract_after_initial_scan(Core::State::AppState& app_state) -> void {
  if (!app_state.settings) {
    return;
  }

  const auto& config = app_state.settings->raw.extensions.infinity_nikki;
  if (!config.allow_online_photo_metadata_extract) {
    return;
  }

  if (Core::Tasks::has_active_task_of_type(app_state,
                                           "extensions.infinityNikki.extractPhotoParams")) {
    // 避免与用户手动触发或其他流程触发的同类任务并发。
    Logger().debug(
        "Skip InfinityNikki full metadata extract after initial scan: extract task is active");
    return;
  }

  auto task_result = Extensions::InfinityNikki::TaskService::start_extract_photo_params_task(
      app_state, InfinityNikkiExtractPhotoParamsRequest{
                     .only_missing = false,
                     .folder_id = std::nullopt,
                     .uid_override = std::nullopt,
                 });
  if (!task_result) {
    Logger().warn("Failed to start InfinityNikki full metadata extract after initial scan: {}",
                  task_result.error());
    return;
  }

  Logger().info("InfinityNikki full metadata extract task started after initial scan: {}",
                task_result.value());
}

auto make_initial_scan_options(const std::filesystem::path& directory)
    -> Features::Gallery::Types::ScanOptions {
  Features::Gallery::Types::ScanOptions options;
  options.directory = directory.string();
  options.ignore_rules = make_infinity_nikki_ignore_rules();
  return options;
}

// 根据当前的系统设置，注册《无限暖暖》照片服务的监听目录与回调。
auto register_impl(Core::State::AppState& app_state, bool start_immediately) -> void {
  auto& state = service_state();
  std::lock_guard<std::mutex> lock(state.mutex);

  auto stop_current_watcher = [&]() {
    if (state.current_watch_path.empty()) {
      return;
    }
    auto remove_result = Features::Gallery::Watcher::remove_watcher_for_directory(
        app_state, state.current_watch_path);
    if (!remove_result) {
      Logger().warn("Failed to remove InfinityNikki gallery watcher for '{}': {}",
                    state.current_watch_path.string(), remove_result.error());
    }
    state.current_watch_path.clear();
  };

  if (!app_state.settings) {
    stop_current_watcher();
    return;
  }

  const auto& config = app_state.settings->raw.extensions.infinity_nikki;
  if (!config.enable || config.game_dir.empty()) {
    stop_current_watcher();
    return;
  }

  if (!config.gallery_guide_seen) {
    Logger().info("Skip InfinityNikki gallery watcher until gallery guide is completed");
    stop_current_watcher();
    return;
  }

  auto dir_result =
      Extensions::InfinityNikki::ScreenshotHardlinks::resolve_watch_directory(app_state);
  if (!dir_result) {
    Logger().warn("Skip InfinityNikki gallery watcher: {}", dir_result.error());
    stop_current_watcher();
    return;
  }

  auto new_watch_path = dir_result.value();
  auto requires_initial_scan =
      state.current_watch_path.empty() || state.current_watch_path != new_watch_path;
  if (!state.current_watch_path.empty() && state.current_watch_path != new_watch_path) {
    stop_current_watcher();
  }

  auto rules_result = ensure_watch_root_ignore_rules(app_state, new_watch_path);
  if (!rules_result) {
    Logger().warn("Failed to prepare InfinityNikki gallery rules for '{}': {}",
                  new_watch_path.string(), rules_result.error());
    return;
  }

  auto callback = [&app_state](const Features::Gallery::Types::ScanResult& result) {
    on_gallery_scan_complete(app_state, result);
  };

  auto register_result =
      Features::Gallery::Watcher::register_watcher_for_directory(app_state, new_watch_path);
  if (!register_result) {
    Logger().warn("Failed to register InfinityNikki gallery watcher for '{}': {}",
                  new_watch_path.string(), register_result.error());
    return;
  }

  auto callback_result = Features::Gallery::Watcher::set_post_scan_callback_for_directory(
      app_state, new_watch_path, std::move(callback));
  if (!callback_result) {
    Logger().warn("Failed to set InfinityNikki gallery watcher callback for '{}': {}",
                  new_watch_path.string(), callback_result.error());
    return;
  }

  if (start_immediately) {
    if (requires_initial_scan) {
      auto task_result = Extensions::InfinityNikki::TaskService::start_initial_scan_task(
          app_state, make_initial_scan_options(new_watch_path),
          [&app_state](const Features::Gallery::Types::ScanResult& result) {
            // 通用后处理（硬链接同步 + 基于 changes 的静默增量提取）
            on_gallery_scan_complete(app_state, result);
            // 首次全量补偿：显式全量元数据提取
            maybe_start_full_extract_after_initial_scan(app_state);
          });
      if (!task_result) {
        Logger().warn("Failed to start InfinityNikki initial scan task for '{}': {}",
                      new_watch_path.string(), task_result.error());
        return;
      }
      Logger().info("InfinityNikki initial scan task started: {}", task_result.value());
    } else {
      auto start_result =
          Features::Gallery::Watcher::start_watcher_for_directory(app_state, new_watch_path, false);
      if (!start_result) {
        Logger().warn("Failed to start InfinityNikki gallery watcher for '{}': {}",
                      new_watch_path.string(), start_result.error());
        return;
      }
    }
  }

  state.current_watch_path = new_watch_path;
  Logger().info("InfinityNikki gallery watcher registered: {}", new_watch_path.string());
}

auto register_from_settings(Core::State::AppState& app_state) -> void {
  register_impl(app_state, false);
}

// 根据当前的系统设置，动态刷新照片服务的监听行为
auto refresh_from_settings(Core::State::AppState& app_state) -> void {
  register_impl(app_state, true);
}

// 在程序退出时执行清理和释放工作
auto shutdown(Core::State::AppState& app_state) -> void {
  auto& state = service_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!state.current_watch_path.empty()) {
    auto remove_result = Features::Gallery::Watcher::remove_watcher_for_directory(
        app_state, state.current_watch_path);
    if (!remove_result) {
      Logger().warn("Failed to remove InfinityNikki gallery watcher during shutdown for '{}': {}",
                    state.current_watch_path.string(), remove_result.error());
    }
    state.current_watch_path.clear();
  }
}

}  // namespace Extensions::InfinityNikki::PhotoService
