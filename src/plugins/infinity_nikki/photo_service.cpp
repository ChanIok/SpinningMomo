module;

module Plugins.InfinityNikki.PhotoService;

import std;
import Core.State;
import Core.Tasks;
import Core.WorkerPool;
import Features.Gallery;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Repository;
import Features.Gallery.Watcher;
import Features.Gallery.Types;
import Features.Settings.State;
import Plugins.InfinityNikki.TaskService;
import Plugins.InfinityNikki.ScreenshotShortcuts;
import Plugins.InfinityNikki.Types;
import Utils.Logger;
import Utils.Path;

namespace Plugins::InfinityNikki::PhotoService {

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

// 每次画廊扫描完毕后触发的回调处理，包含同步快捷方式和提取照片参数
auto on_gallery_scan_complete(Core::State::AppState& app_state,
                              const Features::Gallery::Types::ScanResult& result) -> void {
  if (!app_state.settings) {
    return;
  }

  const auto& config = app_state.settings->raw.plugins.infinity_nikki;

  if (config.manage_screenshot_shortcuts) {
    if (Core::Tasks::has_active_task_of_type(
            app_state, "plugins.infinityNikki.initializeScreenshotShortcuts")) {
      Logger().debug("Skip InfinityNikki screenshot shortcut sync: initialization task is active");
    } else {
      bool submitted = Core::WorkerPool::submit_task(*app_state.worker_pool, [&app_state]() {
        auto sync_result = Plugins::InfinityNikki::ScreenshotShortcuts::sync(app_state);
        if (!sync_result) {
          Logger().warn("InfinityNikki screenshot shortcuts sync failed: {}", sync_result.error());
        } else {
          const auto& r = sync_result.value();
          Logger().info(
              "InfinityNikki screenshot shortcuts synced: source={}, created={}, updated={}, "
              "removed={}, ignored={}",
              r.source_count, r.created_count, r.updated_count, r.removed_count, r.ignored_count);
        }
      });
      if (!submitted) {
        Logger().warn("InfinityNikki shortcuts sync: failed to submit worker task");
      }
    }
  }

  if (config.allow_online_photo_metadata_extract && result.new_items > 0) {
    auto task_result = Plugins::InfinityNikki::TaskService::start_extract_photo_params_task(
        app_state, InfinityNikkiExtractPhotoParamsRequest{.only_missing = true});
    if (!task_result) {
      Logger().warn("InfinityNikki auto photo metadata extract task not started: {}",
                    task_result.error());
    } else {
      Logger().info("InfinityNikki auto photo metadata extract task started: {}",
                    task_result.value());
    }
  }
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

  const auto& config = app_state.settings->raw.plugins.infinity_nikki;
  if (!config.enable || config.game_dir.empty()) {
    stop_current_watcher();
    return;
  }

  if (!config.gallery_guide_seen) {
    Logger().info("Skip InfinityNikki gallery watcher until gallery guide is completed");
    stop_current_watcher();
    return;
  }

  auto dir_result = Plugins::InfinityNikki::ScreenshotShortcuts::resolve_watch_directory(app_state);
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
      auto task_result = Plugins::InfinityNikki::TaskService::start_initial_scan_task(
          app_state, make_initial_scan_options(new_watch_path),
          [&app_state](const Features::Gallery::Types::ScanResult& result) {
            on_gallery_scan_complete(app_state, result);
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

}  // namespace Plugins::InfinityNikki::PhotoService
