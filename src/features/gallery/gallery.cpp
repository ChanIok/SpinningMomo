module;

module Features.Gallery;

import std;
import Core.Async;
import Core.RPC.NotificationHub;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Service;
import Features.Gallery.Scanner;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Service;
import Features.Gallery.RootAvailability;
import Features.Gallery.StaticResolver;
import Features.Gallery.Watcher;
import Utils.File;
import Utils.Image;
import Utils.Logger;
import Utils.LRUCache;
import Utils.Path;
import Utils.String;
import Utils.System;
import <asio.hpp>;
import <mfapi.h>;

namespace Features::Gallery {

auto make_bootstrap_scan_options(const std::filesystem::path& directory) -> Types::ScanOptions {
  Types::ScanOptions options;
  options.directory = directory.string();
  options.supported_extensions = Scanner::Common::default_supported_extensions();
  return options;
}

std::atomic<std::uint64_t> clipboard_temp_file_sequence = 0;

// 在目标目录内生成专用临时路径，让 watcher 不会把尚未完成的文件当作媒体。
auto make_clipboard_temp_path(const std::filesystem::path& target_folder)
    -> std::expected<std::filesystem::path, std::string> {
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto sequence = clipboard_temp_file_sequence.fetch_add(1, std::memory_order_relaxed);
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    auto candidate = target_folder / std::format(".spinningmomo-paste-{}-{}.tmp", ticks, sequence);

    std::error_code exists_error;
    if (!std::filesystem::exists(candidate, exists_error)) {
      if (exists_error) {
        return std::unexpected("Failed to inspect clipboard temporary path: " +
                               exists_error.message());
      }
      return candidate;
    }
  }
  return std::unexpected("Failed to allocate a unique clipboard temporary path");
}

// 为保留原文件名的粘贴生成不覆盖目标，并在重名时追加递增序号。
auto make_unique_clipboard_destination(const std::filesystem::path& target_folder,
                                       const std::filesystem::path& requested_name,
                                       std::uint64_t suffix)
    -> std::expected<std::filesystem::path, std::string> {
  auto filename = requested_name.filename();
  if (filename.empty() || filename == L"." || filename == L"..") {
    return std::unexpected("Clipboard file name is invalid");
  }

  auto candidate = target_folder / filename;
  if (suffix > 0) {
    candidate =
        target_folder / std::filesystem::path(std::format(L"{} ({}){}", filename.stem().wstring(),
                                                          suffix, filename.extension().wstring()));
  }

  auto normalized_result = Utils::Path::NormalizePath(candidate);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize clipboard destination: " +
                           normalized_result.error());
  }
  if (!Utils::Path::IsPathWithinBase(normalized_result.value(), target_folder)) {
    return std::unexpected("Clipboard destination escaped the target folder");
  }
  return normalized_result.value();
}

// 将完整临时文件原子提交到首个空闲名称，绝不覆盖目标目录中的现有文件。
auto commit_clipboard_temp_file(const std::filesystem::path& temporary_path,
                                const std::filesystem::path& target_folder,
                                const std::filesystem::path& requested_name)
    -> std::expected<std::filesystem::path, std::string> {
  for (std::uint64_t suffix = 0; suffix < 10'000; ++suffix) {
    auto destination_result =
        make_unique_clipboard_destination(target_folder, requested_name, suffix);
    if (!destination_result) {
      return std::unexpected(destination_result.error());
    }
    const auto& destination = destination_result.value();

    std::error_code exists_error;
    if (std::filesystem::exists(destination, exists_error)) {
      continue;
    }
    if (exists_error) {
      return std::unexpected("Failed to inspect clipboard destination: " + exists_error.message());
    }

    // rename 在同一目录内提交完整文件；并发创建同名目标时继续尝试下一个名称。
    std::error_code rename_error;
    std::filesystem::rename(temporary_path, destination, rename_error);
    if (!rename_error) {
      return destination;
    }

    std::error_code collision_error;
    if (std::filesystem::exists(destination, collision_error) && !collision_error) {
      continue;
    }
    return std::unexpected("Failed to commit clipboard file: " + rename_error.message());
  }
  return std::unexpected("Too many clipboard file name collisions");
}

// 把编码字节完整写入临时文件，失败时不留下可被图库识别的媒体扩展名。
auto write_clipboard_bytes(const std::filesystem::path& path,
                           const std::vector<std::uint8_t>& bytes)
    -> std::expected<void, std::string> {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) {
    return std::unexpected("Failed to create clipboard temporary file");
  }
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  stream.close();
  if (!stream) {
    return std::unexpected("Failed to write clipboard temporary file");
  }
  return {};
}

// 删除本次尚未提交的临时文件，不影响已经原子改名成功的目标。
auto cleanup_clipboard_temp_file(const std::filesystem::path& temporary_path) -> void {
  if (temporary_path.empty()) {
    return;
  }
  std::error_code cleanup_error;
  std::filesystem::remove(temporary_path, cleanup_error);
  if (cleanup_error) {
    Logger().warn("Failed to clean clipboard temporary file '{}': {}", temporary_path.string(),
                  cleanup_error.message());
  }
}

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

// ============= 资产项管理 =============

auto delete_asset(Core::State::AppState& app_state, const Types::DeleteParams& params)
    -> std::expected<Types::OperationResult, std::string> {
  try {
    // 获取要删除的资产项
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, params.id);
    if (!asset_result) {
      return std::unexpected("Failed to get asset item: " + asset_result.error());
    }

    if (!asset_result->has_value()) {
      Types::OperationResult result;
      result.success = false;
      result.message = "Asset item not found";
      result.affected_count = 0;
      return result;
    }

    auto asset = asset_result->value();

    // 删除物理文件（如果请求）
    if (params.delete_file.value_or(false)) {
      std::filesystem::path file_path(asset.path);
      if (std::filesystem::exists(file_path)) {
        std::error_code ec;
        std::filesystem::remove(file_path, ec);
        if (ec) {
          Logger().warn("Failed to delete physical file {}: {}", asset.path, ec.message());
        } else {
          Logger().info("Deleted physical file: {}", asset.path);
        }
      }
    }

    // 仅删除资产索引；按 hash 共享的缩略图由缓存对账确认无引用后统一回收。
    auto delete_result = Asset::Repository::delete_asset(app_state, params.id);

    Types::OperationResult result;
    if (delete_result) {
      result.success = true;
      result.message = params.delete_file.value_or(false)
                           ? "Asset item and file deleted successfully"
                           : "Asset item removed from library successfully";
      result.affected_count = 1;
    } else {
      result.success = false;
      result.message = "Failed to delete asset item: " + delete_result.error();
      result.affected_count = 0;
    }

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in delete_asset: " + std::string(e.what()));
  }
}

auto open_asset_with_default_app(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<Types::OperationResult, std::string> {
  auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
  if (!asset_result) {
    return std::unexpected("Failed to get asset item: " + asset_result.error());
  }

  if (!asset_result->has_value()) {
    return Types::OperationResult{
        .success = false,
        .message = "Asset item not found",
        .affected_count = 0,
    };
  }

  auto open_result =
      Utils::System::open_file_with_default_app(std::filesystem::path(asset_result->value().path));
  if (!open_result) {
    return std::unexpected("Failed to open asset with default app: " + open_result.error());
  }

  return Types::OperationResult{
      .success = true,
      .message = "Asset opened with default app",
      .affected_count = 0,
  };
}

auto reveal_asset_in_explorer(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<Types::OperationResult, std::string> {
  auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
  if (!asset_result) {
    return std::unexpected("Failed to get asset item: " + asset_result.error());
  }

  if (!asset_result->has_value()) {
    return Types::OperationResult{
        .success = false,
        .message = "Asset item not found",
        .affected_count = 0,
    };
  }

  auto reveal_result =
      Utils::System::reveal_file_in_explorer(std::filesystem::path(asset_result->value().path));
  if (!reveal_result) {
    return std::unexpected("Failed to reveal asset in explorer: " + reveal_result.error());
  }

  return Types::OperationResult{
      .success = true,
      .message = "Asset revealed in explorer",
      .affected_count = 0,
  };
}

auto copy_assets_to_clipboard(Core::State::AppState& app_state,
                              const std::vector<std::int64_t>& ids)
    -> std::expected<Types::OperationResult, std::string> {
  // 这一层负责把“选中的资产 ID”转换成真正可复制的磁盘文件路径。
  // 真正的系统剪贴板写入由 Utils::System 负责。
  if (ids.empty()) {
    return Types::OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  std::vector<std::int64_t> unique_ids;
  unique_ids.reserve(ids.size());
  std::unordered_set<std::int64_t> seen_ids;
  seen_ids.reserve(ids.size());

  // 保持选择集顺序，同时去掉重复 ID，避免重复复制同一个文件。
  for (auto id : ids) {
    if (seen_ids.insert(id).second) {
      unique_ids.push_back(id);
    }
  }

  std::vector<std::filesystem::path> clipboard_paths;
  clipboard_paths.reserve(unique_ids.size());

  std::int64_t copied_count = 0;
  std::int64_t not_found_count = 0;
  std::vector<std::string> errors;
  errors.reserve(unique_ids.size());

  // 逐个把资产 ID 转成文件路径，并过滤掉索引不存在或磁盘不存在的项。
  for (auto id : unique_ids) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
    if (!asset_result) {
      errors.push_back("Failed to query asset " + std::to_string(id) + ": " + asset_result.error());
      continue;
    }

    if (!asset_result->has_value()) {
      not_found_count++;
      continue;
    }

    const auto& asset = asset_result->value();
    if (asset.path.empty()) {
      errors.push_back("Asset path is empty for asset " + std::to_string(asset.id));
      continue;
    }

    std::filesystem::path file_path(asset.path);
    std::error_code ec;
    const bool file_exists = std::filesystem::exists(file_path, ec);
    if (ec) {
      errors.push_back("Failed to access file " + asset.path + ": " + ec.message());
      continue;
    }

    if (!file_exists) {
      not_found_count++;
      continue;
    }

    clipboard_paths.push_back(std::move(file_path));
  }

  // 只有在至少找到一个真实文件时，才真正写入系统剪贴板。
  if (!clipboard_paths.empty()) {
    auto copy_result = Utils::System::copy_files_to_clipboard(clipboard_paths);
    if (!copy_result) {
      errors.push_back("Failed to copy files to clipboard: " + copy_result.error());
    } else {
      copied_count = static_cast<std::int64_t>(clipboard_paths.size());
    }
  }

  const auto total_count = static_cast<std::int64_t>(unique_ids.size());
  const auto failed_count = std::max<std::int64_t>(0, total_count - copied_count - not_found_count);

  // 这里沿用 gallery 现有的 OperationResult 风格，
  // 方便前端统一做 success / partial / failed 的 toast 提示。
  Types::OperationResult result{
      .success = copied_count == total_count,
      .message = "",
      .affected_count = copied_count,
      .failed_count = failed_count,
      .not_found_count = not_found_count,
      .unchanged_count = 0,
  };

  if (result.success) {
    result.message = std::format("Copied {} asset(s) to clipboard", copied_count);
    return result;
  }

  if (copied_count > 0) {
    result.message = std::format("Copied {} asset(s) to clipboard, {} failed, {} not found",
                                 copied_count, failed_count, not_found_count);
  } else {
    result.message = std::format("Failed to copy assets to clipboard: {} failed, {} not found",
                                 failed_count, not_found_count);
  }

  // 详细错误记日志，用户界面只展示汇总结果即可。
  for (const auto& error : errors) {
    Logger().warn("copy_assets_to_clipboard: {}", error);
  }

  return result;
}

// 将剪贴板文件或截图无覆盖地写入指定图库目录，再同步建立资产索引。
auto paste_clipboard_to_folder(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<Types::OperationResult, std::string> {
  if (folder_id <= 0) {
    return Types::OperationResult{
        .success = false,
        .message = "Invalid target folder",
        .affected_count = 0,
    };
  }

  // 目标路径只从已索引 folder_id 解析，RPC 调用方不能指定任意磁盘位置。
  auto folder_result = Folder::Repository::get_folder_by_id(app_state, folder_id);
  if (!folder_result) {
    return std::unexpected("Failed to query clipboard target folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return Types::OperationResult{
        .success = false,
        .message = "Target folder not found",
        .affected_count = 0,
    };
  }

  auto target_result =
      Utils::Path::NormalizePath(std::filesystem::path(folder_result->value().path));
  if (!target_result) {
    return std::unexpected("Failed to normalize clipboard target folder: " + target_result.error());
  }
  const auto target_folder = target_result.value();

  std::error_code target_error;
  if (!std::filesystem::is_directory(target_folder, target_error)) {
    return Types::OperationResult{
        .success = false,
        .message = target_error ? "Failed to access target folder: " + target_error.message()
                                : "Target folder does not exist",
        .affected_count = 0,
    };
  }

  auto clipboard_result = Utils::System::read_clipboard_media();
  if (!clipboard_result) {
    return std::unexpected("Failed to read clipboard media: " + clipboard_result.error());
  }
  auto clipboard = std::move(clipboard_result.value());
  if (clipboard.kind == Utils::System::ClipboardMediaKind::Empty) {
    return Types::OperationResult{
        .success = false,
        .message = "Clipboard does not contain supported media",
        .affected_count = 0,
    };
  }

  std::vector<std::filesystem::path> created_paths;
  std::vector<Types::ScanChange> indexed_changes;
  std::int64_t not_found_count = 0;
  std::int64_t unchanged_count = 0;
  std::int64_t indexed_count = 0;
  std::vector<std::string> errors;

  // 文件列表逐项复制，单个失败不阻断同一批次中的其他媒体。
  if (clipboard.kind == Utils::System::ClipboardMediaKind::Files) {
    created_paths.reserve(clipboard.file_paths.size());
    for (const auto& source : clipboard.file_paths) {
      auto normalized_source_result = Utils::Path::NormalizePath(source);
      if (!normalized_source_result) {
        errors.push_back("Failed to normalize clipboard source '" + source.string() +
                         "': " + normalized_source_result.error());
        continue;
      }
      const auto normalized_source = normalized_source_result.value();

      std::error_code source_error;
      const bool source_exists = std::filesystem::exists(normalized_source, source_error);
      if (source_error) {
        errors.push_back("Failed to access clipboard source '" + normalized_source.string() +
                         "': " + source_error.message());
        continue;
      }
      if (!source_exists) {
        not_found_count++;
        continue;
      }
      const bool is_regular_file =
          std::filesystem::is_regular_file(normalized_source, source_error);
      if (source_error) {
        errors.push_back("Failed to inspect clipboard source '" + normalized_source.string() +
                         "': " + source_error.message());
        continue;
      }
      if (!is_regular_file) {
        unchanged_count++;
        continue;
      }

      const auto& supported_extensions = Scanner::Common::default_supported_extensions();
      if (!Scanner::Common::is_supported_file(normalized_source, supported_extensions)) {
        unchanged_count++;
        continue;
      }

      // 同目录粘贴不制造自身副本，避免把“导入”变成隐式复制命令。
      if (Utils::Path::NormalizeForComparison(normalized_source.parent_path()) ==
          Utils::Path::NormalizeForComparison(target_folder)) {
        unchanged_count++;
        continue;
      }

      auto temporary_result = make_clipboard_temp_path(target_folder);
      if (!temporary_result) {
        errors.push_back(temporary_result.error());
        continue;
      }
      auto temporary_path = temporary_result.value();

      std::error_code copy_error;
      std::filesystem::copy_file(normalized_source, temporary_path,
                                 std::filesystem::copy_options::none, copy_error);
      if (copy_error) {
        cleanup_clipboard_temp_file(temporary_path);
        errors.push_back("Failed to copy clipboard file '" + normalized_source.string() +
                         "': " + copy_error.message());
        continue;
      }

      auto commit_result =
          commit_clipboard_temp_file(temporary_path, target_folder, normalized_source.filename());
      if (!commit_result) {
        cleanup_clipboard_temp_file(temporary_path);
        errors.push_back(commit_result.error());
        continue;
      }
      created_paths.push_back(std::move(commit_result.value()));
    }
  } else {
    // 位图剪贴板只产生一个无损 PNG，并沿用截图文件名格式。
    auto temporary_result = make_clipboard_temp_path(target_folder);
    if (!temporary_result) {
      return std::unexpected(temporary_result.error());
    }
    auto temporary_path = temporary_result.value();

    std::expected<void, std::string> write_result;
    if (clipboard.kind == Utils::System::ClipboardMediaKind::EncodedPng) {
      write_result = write_clipboard_bytes(temporary_path, clipboard.encoded_png);
    } else if (clipboard.kind == Utils::System::ClipboardMediaKind::Bitmap &&
               clipboard.bitmap.has_value()) {
      auto factory_result = Utils::Image::get_thread_wic_factory();
      if (!factory_result) {
        cleanup_clipboard_temp_file(temporary_path);
        return std::unexpected("Failed to create image encoder: " + factory_result.error());
      }
      const auto& bitmap = clipboard.bitmap.value();
      write_result = Utils::Image::save_pixel_data_to_file(
          factory_result->get(), bitmap.bgra_pixels.data(), bitmap.width, bitmap.height,
          bitmap.stride, temporary_path.wstring(), Utils::Image::ImageFormat::PNG);
    } else {
      cleanup_clipboard_temp_file(temporary_path);
      return std::unexpected("Clipboard bitmap payload is missing");
    }

    if (!write_result) {
      cleanup_clipboard_temp_file(temporary_path);
      return std::unexpected("Failed to save clipboard image: " + write_result.error());
    }

    auto requested_name = std::filesystem::path(
        Utils::String::FromUtf8(Utils::String::FormatTimestamp(std::chrono::system_clock::now())));
    auto commit_result = commit_clipboard_temp_file(temporary_path, target_folder, requested_name);
    if (!commit_result) {
      cleanup_clipboard_temp_file(temporary_path);
      return std::unexpected(commit_result.error());
    }
    created_paths.push_back(std::move(commit_result.value()));
  }

  // 主动创建由 Gallery 同步建立索引，watcher 只负责随后可能到达的外部文件通知。
  for (const auto& created_path : created_paths) {
    auto index_result = Scanner::upsert_created_file(app_state, folder_id, created_path);
    if (!index_result) {
      errors.push_back("Failed to index pasted file '" + created_path.string() +
                       "': " + index_result.error());
      continue;
    }

    auto asset_result = Asset::Repository::get_asset_by_path(app_state, created_path.string());
    if (!asset_result) {
      errors.push_back("Failed to verify pasted file index '" + created_path.string() +
                       "': " + asset_result.error());
      continue;
    }
    if (!asset_result->has_value()) {
      unchanged_count++;
      continue;
    }

    indexed_count++;
    indexed_changes.insert(indexed_changes.end(),
                           std::make_move_iterator(index_result->changes.begin()),
                           std::make_move_iterator(index_result->changes.end()));
  }

  // 索引已经落库后再把真实 UPSERT 分发给扩展消费者。
  if (!indexed_changes.empty()) {
    auto dispatch_result = Watcher::dispatch_manual_scan_changes(app_state, indexed_changes);
    if (!dispatch_result) {
      errors.push_back("Failed to dispatch pasted file changes: " + dispatch_result.error());
    }
  }

  const auto affected_count = indexed_count;
  const auto failed_count = static_cast<std::int64_t>(errors.size());
  Types::OperationResult result{
      .success = affected_count > 0 && errors.empty(),
      .message = "",
      .affected_count = affected_count,
      .failed_count = failed_count,
      .not_found_count = not_found_count,
      .unchanged_count = unchanged_count,
  };

  if (result.success) {
    result.message = std::format("Pasted {} clipboard file(s)", affected_count);
  } else if (affected_count > 0) {
    result.message = std::format("Pasted {} clipboard file(s), {} failed, {} missing, {} skipped",
                                 affected_count, failed_count, not_found_count, unchanged_count);
  } else {
    result.message =
        std::format("No clipboard files were pasted: {} failed, {} missing, {} skipped",
                    failed_count, not_found_count, unchanged_count);
  }

  for (const auto& error : errors) {
    Logger().warn("paste_clipboard_to_folder: {}", error);
  }
  return result;
}

auto move_assets_to_trash(Core::State::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<Types::OperationResult, std::string> {
  if (ids.empty()) {
    return Types::OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  struct TrashCandidate {
    Types::Asset asset;
    std::filesystem::path file_path;
    bool file_exists = false;
    bool manual_ignore_registered = false;
  };

  std::unordered_set<std::int64_t> unique_ids(ids.begin(), ids.end());
  std::vector<TrashCandidate> candidates;
  candidates.reserve(unique_ids.size());

  std::int64_t moved_count = 0;
  std::int64_t skipped_not_found = 0;
  std::vector<std::string> errors;
  errors.reserve(unique_ids.size());
  std::vector<Types::ScanChange> manual_changes;
  manual_changes.reserve(unique_ids.size());
  std::unordered_set<std::string> manual_change_keys;
  manual_change_keys.reserve(unique_ids.size());

  auto append_manual_change = [&](const std::filesystem::path& path,
                                  Types::ScanChangeAction action) {
    auto normalized_key = Utils::Path::NormalizeForComparison(path);
    auto action_key =
        action == Types::ScanChangeAction::REMOVE ? std::string("remove:") : std::string("upsert:");
    auto key = action_key + Utils::String::ToUtf8(normalized_key);
    if (!manual_change_keys.insert(key).second) {
      return;
    }
    manual_changes.push_back(Types::ScanChange{
        .path = path.string(),
        .action = action,
    });
  };

  for (auto id : unique_ids) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
    if (!asset_result) {
      errors.push_back("Failed to query asset " + std::to_string(id) + ": " + asset_result.error());
      continue;
    }

    if (!asset_result->has_value()) {
      skipped_not_found++;
      continue;
    }

    const auto& asset = asset_result->value();
    std::filesystem::path file_path(asset.path);
    std::error_code ec;
    bool file_exists = std::filesystem::exists(file_path, ec);
    if (ec) {
      errors.push_back("Failed to access file " + asset.path + ": " + ec.message());
      continue;
    }

    candidates.push_back(TrashCandidate{
        .asset = asset,
        .file_path = std::move(file_path),
        .file_exists = file_exists,
    });
  }

  std::vector<std::filesystem::path> recycle_paths;
  recycle_paths.reserve(candidates.size());
  for (auto& candidate : candidates) {
    if (!candidate.file_exists) {
      continue;
    }

    auto begin_ignore_result = Watcher::begin_manual_file_system_ignore(
        app_state, candidate.file_path, candidate.file_path);
    if (!begin_ignore_result) {
      Logger().warn("Failed to register watcher ignore for recycle-bin move '{}': {}",
                    candidate.file_path.string(), begin_ignore_result.error());
    } else {
      candidate.manual_ignore_registered = true;
    }

    recycle_paths.push_back(candidate.file_path);
  }

  bool recycle_failed = false;
  if (!recycle_paths.empty()) {
    auto recycle_result = Utils::System::move_files_to_recycle_bin(recycle_paths);
    if (!recycle_result) {
      recycle_failed = true;
      errors.push_back("Failed to move files to recycle bin: " + recycle_result.error());
    }
  }

  std::unordered_set<std::int64_t> failed_recycle_ids;
  if (recycle_failed) {
    for (const auto& candidate : candidates) {
      if (!candidate.file_exists) {
        continue;
      }
      failed_recycle_ids.insert(candidate.asset.id);
      errors.push_back("Failed to move file to recycle bin " + candidate.asset.path);
    }
  }

  std::vector<std::int64_t> delete_ids;
  delete_ids.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    if (failed_recycle_ids.contains(candidate.asset.id)) {
      continue;
    }
    delete_ids.push_back(candidate.asset.id);
  }

  // 批量只删除资产索引，共享缩略图由缓存对账确认成为孤儿后统一回收。
  auto delete_result = Asset::Repository::batch_delete_assets_by_ids(app_state, delete_ids);
  if (!delete_result) {
    errors.push_back("Failed to delete asset indexes: " + delete_result.error());
  } else {
    moved_count = static_cast<std::int64_t>(delete_ids.size());
    Logger().info("Gallery move_assets_to_trash: moved {} asset file(s) to recycle bin",
                  moved_count);
    std::unordered_set<std::int64_t> deleted_id_set(delete_ids.begin(), delete_ids.end());
    for (const auto& candidate : candidates) {
      if (!deleted_id_set.contains(candidate.asset.id)) {
        continue;
      }
      append_manual_change(candidate.file_path, Types::ScanChangeAction::REMOVE);
    }

    if (!manual_changes.empty()) {
      Logger().info("Gallery move_assets_to_trash: dispatching {} manual scan change(s)",
                    manual_changes.size());
      auto dispatch_result = Watcher::dispatch_manual_scan_changes(app_state, manual_changes);
      if (!dispatch_result) {
        Logger().warn("Failed to dispatch manual scan changes after trash move: {}",
                      dispatch_result.error());
      }
    }
  }

  for (auto& candidate : candidates) {
    if (!candidate.manual_ignore_registered) {
      continue;
    }
    if (auto complete_ignore_result = Watcher::complete_manual_file_system_ignore(
            app_state, candidate.file_path, candidate.file_path);
        !complete_ignore_result) {
      Logger().warn("Failed to complete watcher ignore for recycle-bin move '{}': {}",
                    candidate.file_path.string(), complete_ignore_result.error());
    }
  }

  Types::OperationResult result{
      .success = errors.empty(),
      .message = "",
      .affected_count = moved_count,
      .failed_count =
          static_cast<std::int64_t>(unique_ids.size()) - moved_count - skipped_not_found,
      .not_found_count = skipped_not_found,
      .unchanged_count = 0,
  };

  if (errors.empty()) {
    result.message = std::format("Moved {} asset(s) to recycle bin", moved_count);
    return result;
  }

  if (moved_count > 0) {
    result.message = std::format("Moved {} asset(s) to recycle bin, {} failed, {} not found",
                                 moved_count, errors.size(), skipped_not_found);
  } else {
    result.message = std::format("Failed to move assets to recycle bin: {} failed, {} not found",
                                 errors.size(), skipped_not_found);
  }

  for (const auto& error : errors) {
    Logger().warn("move_assets_to_trash: {}", error);
  }

  return result;
}

auto move_assets_to_folder(Core::State::AppState& app_state,
                           const Types::MoveAssetsToFolderParams& params)
    -> std::expected<Types::OperationResult, std::string> {
  if (params.ids.empty()) {
    return Types::OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  if (params.target_folder_id <= 0) {
    return Types::OperationResult{
        .success = false,
        .message = "Invalid target folder",
        .affected_count = 0,
    };
  }

  auto target_folder_result =
      Folder::Repository::get_folder_by_id(app_state, params.target_folder_id);
  if (!target_folder_result) {
    return std::unexpected("Failed to query target folder: " + target_folder_result.error());
  }
  if (!target_folder_result->has_value()) {
    return Types::OperationResult{
        .success = false,
        .message = "Target folder not found",
        .affected_count = 0,
    };
  }

  auto normalized_target_folder_result =
      Utils::Path::NormalizePath(std::filesystem::path(target_folder_result->value().path));
  if (!normalized_target_folder_result) {
    return std::unexpected("Failed to normalize target folder path: " +
                           normalized_target_folder_result.error());
  }

  auto target_folder_path = normalized_target_folder_result.value();
  std::unordered_set<std::int64_t> unique_ids(params.ids.begin(), params.ids.end());
  std::int64_t moved_count = 0;
  std::int64_t skipped_not_found = 0;
  std::int64_t skipped_same_folder = 0;
  std::vector<std::string> errors;
  errors.reserve(unique_ids.size());
  std::vector<Types::ScanChange> manual_changes;
  manual_changes.reserve(unique_ids.size() * 2);
  std::unordered_set<std::string> manual_change_keys;
  manual_change_keys.reserve(unique_ids.size() * 2);

  auto append_manual_change = [&](const std::filesystem::path& path,
                                  Types::ScanChangeAction action) {
    auto normalized_key = Utils::Path::NormalizeForComparison(path);
    auto action_key =
        action == Types::ScanChangeAction::REMOVE ? std::string("remove:") : std::string("upsert:");
    auto key = action_key + Utils::String::ToUtf8(normalized_key);
    if (!manual_change_keys.insert(key).second) {
      return;
    }
    manual_changes.push_back(Types::ScanChange{
        .path = path.string(),
        .action = action,
    });
  };

  for (auto id : unique_ids) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
    if (!asset_result) {
      errors.push_back("Failed to query asset " + std::to_string(id) + ": " + asset_result.error());
      continue;
    }
    if (!asset_result->has_value()) {
      skipped_not_found++;
      continue;
    }

    auto asset = asset_result->value();
    auto normalized_source_result = Utils::Path::NormalizePath(std::filesystem::path(asset.path));
    if (!normalized_source_result) {
      errors.push_back("Failed to normalize source path for asset " + std::to_string(asset.id) +
                       ": " + normalized_source_result.error());
      continue;
    }

    auto source_path = normalized_source_result.value();
    auto destination_path = target_folder_path / source_path.filename();
    auto normalized_destination_result = Utils::Path::NormalizePath(destination_path);
    if (!normalized_destination_result) {
      errors.push_back("Failed to normalize destination path for asset " +
                       std::to_string(asset.id) + ": " + normalized_destination_result.error());
      continue;
    }

    auto normalized_destination_path = normalized_destination_result.value();
    // 目标与源一致时不报错，按“跳过项”处理，便于批量操作给出可理解反馈。
    if (Utils::Path::NormalizeForComparison(source_path) ==
        Utils::Path::NormalizeForComparison(normalized_destination_path)) {
      skipped_same_folder++;
      continue;
    }

    bool manual_ignore_registered = false;
    // 先注册 watcher ignore，再执行 move，避免 watcher 抢先对同一路径做重复分析。
    if (auto begin_ignore_result = Watcher::begin_manual_file_system_ignore(
            app_state, source_path, normalized_destination_path);
        begin_ignore_result) {
      manual_ignore_registered = true;
    } else {
      Logger().warn("Failed to register watcher ignore for manual move '{}': {}",
                    source_path.string(), begin_ignore_result.error());
    }

    auto complete_manual_ignore = [&]() {
      if (!manual_ignore_registered) {
        return;
      }
      // 无论成功还是失败都尝试完成 ignore，防止 in-flight 计数泄漏。
      if (auto complete_ignore_result = Watcher::complete_manual_file_system_ignore(
              app_state, source_path, normalized_destination_path);
          !complete_ignore_result) {
        Logger().warn("Failed to complete watcher ignore for manual move '{}': {}",
                      source_path.string(), complete_ignore_result.error());
      }
      manual_ignore_registered = false;
    };

    auto move_result =
        Utils::File::move_path_blocking(source_path, normalized_destination_path, false);
    if (!move_result) {
      complete_manual_ignore();
      errors.push_back("Failed to move asset " + std::to_string(asset.id) + ": " +
                       move_result.error());
      continue;
    }

    // 文件系统移动成功后再更新索引，保证 DB 记录与磁盘最终位置保持一致。
    auto update_result = Asset::Repository::update_asset_location(
        app_state, asset.id, normalized_destination_path.filename().string(),
        normalized_destination_path.generic_string(), params.target_folder_id);
    if (!update_result) {
      complete_manual_ignore();
      errors.push_back("Failed to update asset index " + std::to_string(asset.id) + ": " +
                       update_result.error());
      continue;
    }

    complete_manual_ignore();

    append_manual_change(source_path, Types::ScanChangeAction::REMOVE);
    append_manual_change(normalized_destination_path, Types::ScanChangeAction::UPSERT);
    moved_count++;
  }

  if (!manual_changes.empty()) {
    auto dispatch_result = Watcher::dispatch_manual_scan_changes(app_state, manual_changes);
    if (!dispatch_result) {
      Logger().warn("Failed to dispatch manual scan changes after folder move: {}",
                    dispatch_result.error());
    }
  }

  Types::OperationResult result{
      .success = errors.empty(),
      .message = "",
      .affected_count = moved_count,
      .failed_count = static_cast<std::int64_t>(unique_ids.size()) - moved_count -
                      skipped_not_found - skipped_same_folder,
      .not_found_count = skipped_not_found,
      .unchanged_count = skipped_same_folder,
  };

  if (errors.empty()) {
    result.message = std::format("Moved {} asset(s) to target folder", moved_count);
    return result;
  }

  if (moved_count > 0) {
    result.message =
        std::format("Moved {} asset(s), {} failed, {} not found, {} already in target folder",
                    moved_count, errors.size(), skipped_not_found, skipped_same_folder);
  } else {
    result.message = std::format("Failed to move assets: {} failed, {} not found, {} unchanged",
                                 errors.size(), skipped_not_found, skipped_same_folder);
  }

  for (const auto& error : errors) {
    Logger().warn("move_assets_to_folder: {}", error);
  }

  return result;
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
