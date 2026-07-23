module;

module Features.Gallery;

import std;
import Core.Async;
import Core.RPC.NotificationHub;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Scanner;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Folder.Service;
import Features.Gallery.RootAvailability;
import Features.Gallery.StaticResolver;
import Features.Gallery.Watcher;
import Utils.Logger;
import Utils.Path;
import <asio.hpp>;
import <mfapi.h>;

namespace Features::Gallery {

// 为输出目录补齐 Gallery 默认支持的媒体扩展名。
auto make_bootstrap_scan_options(const std::filesystem::path& directory) -> Types::ScanOptions {
  Types::ScanOptions options;
  options.directory = directory.string();
  options.supported_extensions = Scanner::Common::default_supported_extensions();
  return options;
}

// 异步扫描当前输出目录，并把它接入 Gallery 的持续监听。
auto ensure_output_directory_media_source(Core::State::AppState& app_state,
                                          const std::string& output_dir_path) -> void {
  if (!app_state.async) {
    Logger().warn("Skip output-directory gallery sync: async state is not ready");
    return;
  }

  auto* io_context = Core::Async::get_io_context(app_state);
  if (!io_context) {
    Logger().warn("Skip output-directory gallery sync: async runtime is not available");
    return;
  }

  auto output_dir_path_snapshot = output_dir_path;
  asio::co_spawn(
      *io_context,
      [&app_state, output_dir_path_snapshot]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);

        auto output_dir_result = Utils::Path::GetOutputDirectory(output_dir_path_snapshot);
        if (!output_dir_result) {
          Logger().warn("Failed to resolve output directory for gallery sync: {}",
                        output_dir_result.error());
        } else {
          auto scan_result =
              scan_directory(app_state, make_bootstrap_scan_options(output_dir_result.value()));
          if (!scan_result) {
            Logger().warn("Failed to scan output directory for gallery sync '{}': {}",
                          output_dir_result->string(), scan_result.error());
          } else {
            Logger().info("Output directory added to gallery sources: {}",
                          output_dir_result->string());
            Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
          }
        }
      },
      asio::detached_t{});
}

// ============= 初始化和清理 =============

// 准备 Gallery 运行资源：媒体运行时、缩略图目录、根可达性和静态映射。
auto prepare_runtime_resources(Core::State::AppState& app_state)
    -> std::expected<void, std::string> {
  try {
    Logger().info("Initializing gallery module...");

    // 供 Utils::Media::VideoAsset（SourceReader）使用；须在任意 analyze 之前成功。
    if (FAILED(MFStartup(MF_VERSION))) {
      return std::unexpected("Failed to initialize Media Foundation for gallery");
    }

    // 确保缩略图目录存在
    auto ensure_dir_result = Asset::Thumbnail::ensure_thumbnails_directory_exists(app_state);
    if (!ensure_dir_result) {
      Logger().error("Failed to ensure thumbnails directory exists: {}", ensure_dir_result.error());
      return std::unexpected("Failed to ensure thumbnails directory exists: " +
                             ensure_dir_result.error());
    }

    if (auto availability_result = Features::Gallery::RootAvailability::initialize(app_state);
        !availability_result) {
      return std::unexpected("Failed to initialize gallery root availability: " +
                             availability_result.error());
    }

    // 注册静态服务解析器
    StaticResolver::register_http_resolvers(app_state);
    StaticResolver::register_webview_resolvers(app_state);

    // 根据数据库里的根文件夹记录，确保 WebView 原图 host mappings 全部就绪。
    if (auto mapping_result = Folder::Service::ensure_all_root_folder_webview_mappings(app_state);
        !mapping_result) {
      return std::unexpected("Failed to sync gallery root WebView mappings: " +
                             mapping_result.error());
    }

    Logger().info("Gallery module initialized successfully");
    Logger().info("Thumbnail directory set to: {}",
                  app_state.gallery->thumbnails_directory.string());
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception during asset module initialization: " +
                           std::string(e.what()));
  }
}

// 执行 Gallery 后台启动任务：准备资源后恢复 watcher，并让外部扩展接入。
auto run_startup_task(Core::State::AppState& app_state,
                      std::function<void(Core::State::AppState&)> after_ready,
                      std::stop_token stop_token) -> void {
  try {
    Logger().info("Gallery startup initialization started");

    auto init_result = prepare_runtime_resources(app_state);
    if (!init_result) {
      Logger().warn("Gallery startup initialization failed: {}", init_result.error());
      return;
    }

    // 退出已开始时不再注册 watcher 和扩展回调，避免和清理流程交错。
    if (stop_token.stop_requested()) {
      Logger().info("Stop Gallery startup initialization: shutdown has been requested");
      return;
    }

    if (auto watcher_restore_result =
            Features::Gallery::Watcher::restore_watchers_from_db(app_state);
        !watcher_restore_result) {
      Logger().warn("Gallery watcher registration restore failed: {}",
                    watcher_restore_result.error());
    }

    // Gallery 根状态就绪后再通知外部扩展接入同一套 watcher。
    if (after_ready) {
      after_ready(app_state);
    }

    if (!stop_token.stop_requested()) {
      // 复用 Gallery 启动线程串行恢复，避免占用并等待扫描内部使用的 WorkerPool。
      auto watcher_start_result = Features::Gallery::Watcher::start_registered_watchers(app_state);
      if (!watcher_start_result && !stop_token.stop_requested()) {
        Logger().warn("Gallery watcher startup recovery failed: {}", watcher_start_result.error());
      }
    }

    Logger().info("Gallery startup initialization completed");
  } catch (const std::exception& e) {
    Logger().warn("Gallery startup initialization crashed: {}", e.what());
  } catch (...) {
    Logger().warn("Gallery startup initialization crashed with unknown error");
  }
}

// 启动 Gallery 模块；慢启动链路在模块自己的后台线程中继续推进。
auto initialize(Core::State::AppState& app_state,
                std::function<void(Core::State::AppState&)> after_ready)
    -> std::expected<void, std::string> {
  try {
    app_state.gallery->shutdown_requested.store(false, std::memory_order_release);
    // 每次初始化创建新的停止源，保证本轮 Gallery 扫描拿到未停止的 token。
    app_state.gallery->scan_stop_source = std::stop_source{};
    app_state.gallery->startup_initialization_thread = std::jthread(
        [&app_state, after_ready = std::move(after_ready)](std::stop_token stop_token) mutable {
          run_startup_task(app_state, std::move(after_ready), stop_token);
        });
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to start Gallery startup initialization thread: " +
                           std::string(e.what()));
  }
}

// 清理 Gallery 模块：先收敛后台启动和 watcher，再释放资源。
auto cleanup(Core::State::AppState& app_state,
             std::function<void(Core::State::AppState&)> before_watchers_shutdown) -> void {
  try {
    Logger().info("Cleaning up gallery module resources...");

    // 先通知所有扫描停止，避免退出阶段继续读取文件或生成缩略图。
    app_state.gallery->shutdown_requested.store(true, std::memory_order_release);
    app_state.gallery->scan_stop_source.request_stop();

    // 等待启动线程结束，避免它继续注册 WebView 映射或 watcher。
    app_state.gallery->startup_initialization_thread.request_stop();
    if (app_state.gallery->startup_initialization_thread.joinable()) {
      app_state.gallery->startup_initialization_thread.join();
    }

    // 外部扩展先解绑自己的 watcher，再由 Gallery 统一关闭剩余 watcher。
    if (before_watchers_shutdown) {
      before_watchers_shutdown(app_state);
    }
    Features::Gallery::Watcher::shutdown_watchers(app_state);

    // 等所有扫描离开共享区后再释放 Media Foundation 和缩略图路径等运行资源。
    std::unique_lock<std::shared_mutex> scan_lifetime_lock(app_state.gallery->scan_lifetime_mutex);

    // 注销静态服务解析器
    StaticResolver::unregister_all_resolvers(app_state);

    // 重置缩略图路径状态
    app_state.gallery->thumbnails_directory.clear();

    // 与 initialize 中 MFStartup 成对；此后不应再调用视频分析。
    MFShutdown();

    Logger().info("Gallery module cleanup completed");
  } catch (const std::exception& e) {
    Logger().error("Exception during asset module cleanup: {}", e.what());
  }
}

// ============= 扫描和索引 =============

// 扫描指定目录并补齐缺失缩略图，成功后确保对应 watcher 已注册
auto scan_directory(Core::State::AppState& app_state, const Types::ScanOptions& options,
                    std::function<void(const Types::ScanProgress&)> progress_callback)
    -> std::expected<Types::ScanResult, std::string> {
  auto stop_token = app_state.gallery->scan_stop_source.get_token();
  // 一个业务扫描全程持有共享锁，cleanup 会在释放媒体资源前等待它结束。
  std::shared_lock<std::shared_mutex> scan_lifetime_lock(app_state.gallery->scan_lifetime_mutex);
  if (stop_token.stop_requested()) {
    return std::unexpected("Asset scan cancelled");
  }

  auto scan_result =
      Scanner::scan_asset_directory(app_state, options, std::move(progress_callback));
  if (!scan_result) {
    Logger().error("Asset scan failed: {}", scan_result.error());
    return std::unexpected("Asset scan failed: " + scan_result.error());
  }

  auto result = scan_result.value();
  Logger().info("Asset scan completed. Total: {}, New: {}, Updated: {}, Errors: {}",
                result.total_files, result.new_items, result.updated_items, result.errors.size());

  if (stop_token.stop_requested()) {
    return std::unexpected("Asset scan cancelled");
  }

  // 手动/显式扫描后只做当前目录的“缺失缩略图补回”，
  // 不在这里顺手做全局孤儿清理，避免把启动级别的缓存对账混进日常扫描。
  if (!options.rebuild_thumbnails.value_or(false)) {
    auto thumbnail_repair_result = Asset::Thumbnail::repair_missing_thumbnails(
        app_state, std::filesystem::path(options.directory), Types::kDefaultThumbnailShortEdge);
    if (!thumbnail_repair_result) {
      Logger().warn("Gallery thumbnail repair failed after scan '{}': {}", options.directory,
                    thumbnail_repair_result.error());
    } else {
      const auto& stats = thumbnail_repair_result.value();
      Logger().info(
          "Gallery thumbnail repair finished. context=scan_directory, candidates={}, missing={}, "
          "repaired={}, failed={}, skipped_missing_sources={}",
          stats.candidate_hashes, stats.missing_thumbnails, stats.repaired_thumbnails,
          stats.failed_repairs, stats.skipped_missing_sources);
    }
  }

  // 退出已开始时不再注册新的 watcher，避免与 shutdown_watchers 交错。
  if (stop_token.stop_requested()) {
    return std::unexpected("Asset scan cancelled");
  }

  auto watcher_result = Watcher::register_watcher_for_directory(
      app_state, std::filesystem::path(options.directory), options);
  if (!watcher_result) {
    // 扫描已经成功，监听失败这里只记日志，不中断流程。
    Logger().warn("Failed to ensure watcher for '{}': {}", options.directory,
                  watcher_result.error());
    return result;
  }

  auto start_result = Watcher::start_watcher_for_directory(
      app_state, std::filesystem::path(options.directory), false);
  if (!start_result) {
    Logger().warn("Failed to start watcher for '{}': {}", options.directory, start_result.error());
  }

  return result;
}

// 清理已经没有任何资产引用的缩略图文件。
auto cleanup_thumbnails(Core::State::AppState& app_state)
    -> std::expected<Types::OperationResult, std::string> {
  try {
    auto cleanup_result = Asset::Thumbnail::cleanup_orphaned_thumbnails(app_state);

    Types::OperationResult result;
    if (cleanup_result) {
      result.success = true;
      result.message = std::format("Cleaned up {} orphaned thumbnails", cleanup_result.value());
      result.affected_count = cleanup_result.value();
      Logger().info("Thumbnail cleanup completed: {} files removed", cleanup_result.value());
    } else {
      result.success = false;
      result.message = "Failed to cleanup thumbnails: " + cleanup_result.error();
      result.affected_count = 0;
    }

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in cleanup_thumbnails: " + std::string(e.what()));
  }
}

// ============= 统计和信息 =============

// 汇总缩略图目录的文件数量、占用空间和异常状态。
auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string> {
  try {
    auto stats_result = Asset::Thumbnail::get_thumbnail_stats(app_state);
    if (!stats_result) {
      return std::unexpected(stats_result.error());
    }

    auto stats = stats_result.value();

    std::string formatted_stats = std::format(
        "Thumbnail Statistics:\\n"
        "Directory: {}\\n"
        "Total Thumbnails: {}\\n"
        "Total Size: {} bytes\\n"
        "Orphaned Thumbnails: {}\\n"
        "Corrupted Thumbnails: {}",
        stats.thumbnails_directory, stats.total_thumbnails, stats.total_size,
        stats.orphaned_thumbnails, stats.corrupted_thumbnails);

    return formatted_stats;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_thumbnail_stats: " + std::string(e.what()));
  }
}

}  // namespace Features::Gallery
