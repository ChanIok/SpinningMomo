module;

module Features.Gallery.Scanner;

import std;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Scanner.Progress;
import Features.Gallery.Scanner.Discovery;
import Features.Gallery.Scanner.Analysis;
import Features.Gallery.Scanner.Process;
import Features.Gallery.Scanner.Cleanup;
import Features.Gallery.Asset.Service;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Repository;
import Utils.Logger;
import Utils.Path;

namespace Features::Gallery::Scanner {

struct ScanPreparationContext {
  std::filesystem::path normalized_scan_root;
  std::filesystem::path directory;
  std::int64_t folder_id = 0;
  std::unordered_map<std::string, Types::Metadata> asset_cache;
};

// 准备扫描上下文：规范化根路径 → 建 root folder → 可选持久化 ignore → 加载 asset cache
auto prepare_scan_context(Core::State::AppState& app_state, const Types::ScanOptions& options)
    -> std::expected<ScanPreparationContext, std::string> {
  auto normalized_scan_root_result = Utils::Path::ResolvePath(options.directory);
  if (!normalized_scan_root_result) {
    return std::unexpected("Failed to normalize scan root path: " +
                           normalized_scan_root_result.error());
  }
  auto normalized_scan_root = normalized_scan_root_result.value();

  Logger().info("Starting folder-aware asset scan for directory '{}' with {} ignore rules",
                normalized_scan_root.string(),
                options.ignore_rules.value_or(std::vector<Types::ScanIgnoreRule>{}).size());

  // 确保扫描根在 folder 表中存在
  std::vector<std::filesystem::path> root_folder_paths = {normalized_scan_root};
  auto root_folder_mapping_result =
      Folder::Service::batch_create_folders_for_paths(app_state, root_folder_paths);
  if (!root_folder_mapping_result) {
    return std::unexpected("Failed to create root folder record: " +
                           root_folder_mapping_result.error());
  }

  auto root_folder_map = std::move(root_folder_mapping_result.value());
  std::int64_t folder_id = root_folder_map.at(normalized_scan_root.string());

  // 有传入 ignore 则整表替换；未传则保留库中已有规则
  if (options.ignore_rules.has_value()) {
    auto persist_result = Ignore::Repository::replace_rules_by_folder_id(
        app_state, folder_id, options.ignore_rules.value());
    if (!persist_result) {
      return std::unexpected("Failed to persist ignore rules: " + persist_result.error());
    }
    Logger().info("Persisted {} ignore rules for folder_id {}", options.ignore_rules->size(),
                  folder_id);
  } else {
    Logger().debug("No ignore rules provided for '{}', keeping existing rules",
                   normalized_scan_root.string());
  }

  auto asset_cache_result = Asset::Service::load_asset_cache(app_state);
  if (!asset_cache_result) {
    return std::unexpected("Failed to load asset cache: " + asset_cache_result.error());
  }

  return ScanPreparationContext{
      .normalized_scan_root = normalized_scan_root,
      .directory = normalized_scan_root,
      .folder_id = folder_id,
      .asset_cache = std::move(asset_cache_result.value()),
  };
}

// 全量同步一个目录：准备 → 发现 → 指纹判定 → 处理落库 → 清理 → 组装 ScanChange
auto scan_asset_directory(Core::State::AppState& app_state, const Types::ScanOptions& options,
                          std::function<void(const Types::ScanProgress&)> progress_callback)
    -> std::expected<Types::ScanResult, std::string> {
  auto stop_token = app_state.gallery->scan_stop_source.get_token();
  // 已取消则直接退出
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  auto start_time = std::chrono::steady_clock::now();
  Progress::report_scan_progress(progress_callback, "preparing", 0, 1, Progress::kPreparingPercent,
                                 "Preparing gallery scan context");

  // 1. 准备：规范化根路径、写 ignore、加载 asset cache
  auto context_result = prepare_scan_context(app_state, options);
  if (!context_result) {
    return std::unexpected(context_result.error());
  }
  auto context = std::move(context_result.value());

  // 准备期间收到停止后不再开始目录发现
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  // 2. 发现：按扩展名与 ignore 枚举磁盘文件
  auto file_infos_result = Discovery::run_discovery_phase(
      app_state, context.directory, context.folder_id, options, progress_callback);
  if (!file_infos_result) {
    return std::unexpected(file_infos_result.error());
  }
  auto file_infos = std::move(file_infos_result.value());

  // 目录发现完成后再次响应停止，避免继续提交内容指纹任务
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  // 3. 指纹：粗判变更，为候选文件算 hash，得到 NEW/MODIFIED 列表
  auto files_to_process_result = Analysis::run_hash_analysis_phase(
      app_state, file_infos, context.asset_cache, options, progress_callback, stop_token);
  if (!files_to_process_result) {
    return std::unexpected(files_to_process_result.error());
  }
  auto files_to_process = std::move(files_to_process_result.value());

  // 4. 处理：元数据/缩略图/主色 + 批量写库
  auto processing_result = Process::run_processing_phase(
      app_state, context.directory, files_to_process, options, progress_callback, stop_token);
  if (!processing_result) {
    return std::unexpected(processing_result.error());
  }
  auto processing_phase = std::move(processing_result.value());

  // 取消后不能进入删除对账，否则不完整的扫描快照可能被当成真实磁盘状态
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  // 5. 清理：DB 有盘无的资产与空目录
  auto cleanup_phase = Cleanup::run_cleanup_phase(
      app_state, context.normalized_scan_root, file_infos, context.asset_cache, progress_callback);

  // 6. 组装统计与 ScanChange（REMOVE / UPSERT，路径去重）
  Types::ScanResult result{
      .total_files = static_cast<int>(file_infos.size()),
      .new_items = static_cast<int>(processing_phase.batch_result.new_assets.size()),
      .updated_items = static_cast<int>(processing_phase.batch_result.updated_assets.size()),
      .deleted_items = cleanup_phase.deleted_items,
      .errors = std::move(processing_phase.batch_result.errors),
  };

  std::unordered_set<std::string> emitted_change_keys;
  emitted_change_keys.reserve(result.new_items + result.updated_items +
                              cleanup_phase.removed_paths.size());

  auto append_scan_change = [&](const std::string& path, Types::ScanChangeAction action) {
    auto key =
        std::string(action == Types::ScanChangeAction::REMOVE ? "remove:" : "upsert:") + path;
    if (!emitted_change_keys.insert(key).second) {
      return;
    }

    result.changes.push_back(Types::ScanChange{
        .path = path,
        .action = action,
    });
  };

  for (const auto& removed_path : cleanup_phase.removed_paths) {
    append_scan_change(removed_path, Types::ScanChangeAction::REMOVE);
  }

  for (const auto& entry : processing_phase.batch_result.new_assets) {
    append_scan_change(entry.asset.path, Types::ScanChangeAction::UPSERT);
  }

  for (const auto& entry : processing_phase.batch_result.updated_assets) {
    append_scan_change(entry.asset.path, Types::ScanChangeAction::UPSERT);
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  result.scan_duration = std::format("{}ms", duration.count());

  if (!processing_phase.all_db_success) {
    result.errors.push_back("Some database operations failed");
  }

  Logger().info(
      "Folder-aware asset scan completed. Total: {}, New: {}, Updated: {}, Deleted: {}, Errors: "
      "{}, Duration: {}",
      result.total_files, result.new_items, result.updated_items, result.deleted_items,
      result.errors.size(), result.scan_duration);

  Progress::report_scan_progress(
      progress_callback, "completed", static_cast<std::int64_t>(result.total_files),
      static_cast<std::int64_t>(result.total_files), 100.0, "Gallery scan completed");

  return result;
}

}  // namespace Features::Gallery::Scanner
