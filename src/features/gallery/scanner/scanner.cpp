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
import Features.Gallery.Scanner.Common;
import Features.Gallery.Scanner.AssetPipeline;
import Features.Gallery.Asset.Service;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import Features.Gallery.Ignore.Repository;
import Features.Gallery.Ignore.Service;
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

// 全量同步一个目录：准备 → 盘点文件/目录 → 同步目录库存 → 处理资产 → 清理 → 组装 ScanChange。
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

  // 2. 发现：一次遍历同时产出媒体文件和真实目录库存。
  auto discovery_result = Discovery::run_discovery_phase(
      app_state, context.directory, context.folder_id, options, progress_callback);
  if (!discovery_result) {
    return std::unexpected(discovery_result.error());
  }
  auto discovery = std::move(discovery_result.value());
  auto& file_infos = discovery.file_infos;

  // 原路径重新出现即恢复同一资产；不因 size/mtime/hash 是否变化而更换资产 ID。
  std::vector<std::int64_t> restored_asset_ids;
  std::vector<std::string> restored_paths;
  restored_asset_ids.reserve(file_infos.size());
  restored_paths.reserve(file_infos.size());
  for (const auto& file_info : file_infos) {
    auto cached = context.asset_cache.find(file_info.path.string());
    if (cached != context.asset_cache.end() && cached->second.missing_at.has_value()) {
      restored_asset_ids.push_back(cached->second.id);
      restored_paths.push_back(cached->second.path);
      cached->second.missing_at.reset();
    }
  }
  auto restore_result = Asset::Repository::restore_assets_by_ids(app_state, restored_asset_ids);
  if (!restore_result) {
    return std::unexpected("Failed to restore present assets: " + restore_result.error());
  }

  // 目录发现完成后再次响应停止，避免继续提交内容指纹任务
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  // 3. 目录：先物化本次有效目录库存，空目录也能立即进入文件夹树。
  auto folder_mapping_result =
      Folder::Service::batch_create_folders_for_paths(app_state, discovery.folder_paths);
  if (!folder_mapping_result) {
    return std::unexpected("Failed to synchronize folder inventory: " +
                           folder_mapping_result.error());
  }
  auto folder_mapping = std::move(folder_mapping_result.value());

  // 4. 指纹：粗判变更，为候选文件算 hash，得到 NEW/MODIFIED 列表。
  auto files_to_process_result = Analysis::run_hash_analysis_phase(
      app_state, file_infos, context.asset_cache, options, progress_callback, stop_token);
  if (!files_to_process_result) {
    return std::unexpected(files_to_process_result.error());
  }
  auto files_to_process = std::move(files_to_process_result.value());

  // 5. 处理：复用完整目录映射写入元数据、缩略图和主色。
  auto processing_result = Process::run_processing_phase(
      app_state, files_to_process, folder_mapping, options, progress_callback, stop_token);
  if (!processing_result) {
    return std::unexpected(processing_result.error());
  }
  auto processing_phase = std::move(processing_result.value());

  // 取消后不能进入删除对账，否则不完整的扫描快照可能被当成真实磁盘状态
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  // 6. 清理：文件与目录分别以本次盘点库存删除过期索引。
  auto cleanup_phase =
      Cleanup::run_cleanup_phase(app_state, context.normalized_scan_root, file_infos,
                                 discovery.folder_paths, context.asset_cache, progress_callback);

  // 7. 只为媒体资产组装 ScanChange，目录库存变化不泄漏给扩展消费者。
  std::unordered_set<std::string> processed_updated_paths;
  for (const auto& entry : processing_phase.batch_result.updated_assets) {
    processed_updated_paths.insert(entry.asset.path);
  }
  const auto restore_only_count = static_cast<int>(
      std::ranges::count_if(restored_paths, [&processed_updated_paths](const std::string& path) {
        return !processed_updated_paths.contains(path);
      }));

  Types::ScanResult result{
      .total_files = static_cast<int>(file_infos.size()),
      .new_items = static_cast<int>(processing_phase.batch_result.new_assets.size()),
      .updated_items = static_cast<int>(processing_phase.batch_result.updated_assets.size()) +
                       restore_only_count,
      .missing_items = cleanup_phase.missing_items,
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

  for (const auto& restored_path : restored_paths) {
    append_scan_change(restored_path, Types::ScanChangeAction::UPSERT);
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

  Logger().info(
      "Folder-aware asset scan completed. Total: {}, New: {}, Updated: {}, Missing: {}, Errors: "
      "{}, Duration: {}",
      result.total_files, result.new_items, result.updated_items, result.missing_items,
      result.errors.size(), result.scan_duration);

  Progress::report_scan_progress(
      progress_callback, "completed", static_cast<std::int64_t>(result.total_files),
      static_cast<std::int64_t>(result.total_files), 100.0, "Gallery scan completed");

  return result;
}

// 同步物化应用主动创建的单个文件，并返回已经落库的真实扫描变化。
auto upsert_created_file(Core::State::AppState& app_state, std::int64_t folder_id,
                         const std::filesystem::path& path)
    -> std::expected<Types::ScanResult, std::string> {
  auto stop_token = app_state.gallery->scan_stop_source.get_token();
  // 单文件主动导入与常规扫描共享媒体资源，cleanup 会等待本次处理自然结束。
  std::shared_lock<std::shared_mutex> scan_lifetime_lock(app_state.gallery->scan_lifetime_mutex);
  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  auto folder_result = Folder::Repository::get_folder_by_id(app_state, folder_id);
  if (!folder_result) {
    return std::unexpected("Failed to query target folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return std::unexpected("Target folder not found");
  }

  auto folder_path_result =
      Utils::Path::NormalizePath(std::filesystem::path(folder_result->value().path));
  if (!folder_path_result) {
    return std::unexpected("Failed to normalize target folder: " + folder_path_result.error());
  }
  auto file_path_result = Utils::Path::NormalizePath(path);
  if (!file_path_result) {
    return std::unexpected("Failed to normalize created file: " + file_path_result.error());
  }
  const auto folder_path = folder_path_result.value();
  const auto file_path = file_path_result.value();
  if (Utils::Path::NormalizeForComparison(file_path.parent_path()) !=
      Utils::Path::NormalizeForComparison(folder_path)) {
    return std::unexpected("Created file does not belong to the target folder");
  }

  auto root_id_result = Ignore::Service::resolve_root_folder_id(app_state, folder_id);
  if (!root_id_result) {
    return std::unexpected("Failed to resolve target watch root: " + root_id_result.error());
  }
  auto root_result = Folder::Repository::get_folder_by_id(app_state, root_id_result.value());
  if (!root_result) {
    return std::unexpected("Failed to query target watch root: " + root_result.error());
  }
  if (!root_result->has_value()) {
    return std::unexpected("Target watch root not found");
  }

  auto root_path_result =
      Utils::Path::NormalizePath(std::filesystem::path(root_result->value().path));
  if (!root_path_result) {
    return std::unexpected("Failed to normalize target watch root: " + root_path_result.error());
  }
  auto rules_result = Ignore::Service::load_ignore_rules(app_state, folder_id);
  if (!rules_result) {
    return std::unexpected("Failed to load target ignore rules: " + rules_result.error());
  }

  Types::ScanOptions options{
      .directory = root_path_result->string(),
      .supported_extensions = Common::default_supported_extensions(),
  };
  std::unordered_map<std::string, std::int64_t> folder_mapping{
      {folder_path.string(), folder_id},
  };

  // 复用全量与 watcher 共用的单路径管线，避免主动导入另写资产分析逻辑。
  auto upsert_result = AssetPipeline::upsert_asset_at_path(app_state, root_path_result.value(),
                                                           options, rules_result.value(),
                                                           folder_mapping, file_path, stop_token);
  if (!upsert_result) {
    return std::unexpected(upsert_result.error());
  }

  Types::ScanResult result{
      .total_files = 1,
      .scan_duration = "manual_upsert",
  };
  using AssetPipeline::PathSyncOutcome;
  switch (upsert_result.value()) {
    case PathSyncOutcome::Created:
      result.new_items = 1;
      result.changes.push_back(
          Types::ScanChange{.path = file_path.string(), .action = Types::ScanChangeAction::UPSERT});
      break;
    case PathSyncOutcome::Updated:
    case PathSyncOutcome::Restored:
      result.updated_items = 1;
      result.changes.push_back(
          Types::ScanChange{.path = file_path.string(), .action = Types::ScanChangeAction::UPSERT});
      break;
    case PathSyncOutcome::Missing:
      result.missing_items = 1;
      result.changes.push_back(
          Types::ScanChange{.path = file_path.string(), .action = Types::ScanChangeAction::REMOVE});
      break;
    case PathSyncOutcome::Skipped:
    case PathSyncOutcome::UnchangedMeta:
      break;
  }
  return result;
}

}  // namespace Features::Gallery::Scanner
