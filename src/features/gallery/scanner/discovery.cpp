module;

module Features.Gallery.Scanner.Discovery;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Scanner.Progress;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Ignore.Service;
import Utils.Logger;
import Utils.Path;
import Utils.Time;

namespace Features::Gallery::Scanner::Discovery {

struct DiscoveredPaths {
  std::vector<std::filesystem::path> files;
  std::vector<std::filesystem::path> folders;
  std::unordered_set<std::string> folder_keys;
};

// 把目录及其到扫描根的必要祖先加入目录库存。
auto append_folder_with_ancestors(DiscoveredPaths& paths, const std::filesystem::path& folder_path,
                                  const std::filesystem::path& scan_root) -> void {
  auto current = folder_path;
  while (!current.empty() && Utils::Path::IsPathWithinBase(current, scan_root)) {
    auto key = current.string();
    if (paths.folder_keys.insert(key).second) {
      paths.folders.push_back(current);
    }

    // 根目录已经形成完整链路，不再继续向上走。
    if (current == scan_root) {
      break;
    }
    current = current.parent_path();
  }
}

// 一次遍历收集候选文件和可见目录，同时保留深层 include 所需的祖先链。
auto scan_paths(Core::State::AppState& app_state, const std::filesystem::path& directory,
                const Types::ScanOptions& options, std::int64_t folder_id)
    -> std::expected<DiscoveredPaths, std::string> {
  if (!std::filesystem::exists(directory)) {
    return std::unexpected("Directory does not exist: " + directory.string());
  }
  if (!std::filesystem::is_directory(directory)) {
    return std::unexpected("Path is not a directory: " + directory.string());
  }

  try {
    // 规则始终相对顶层监听根匹配，子目录扫描也不改变这个基准。
    auto root_folder_id_result = Ignore::Service::resolve_root_folder_id(app_state, folder_id);
    if (!root_folder_id_result) {
      return std::unexpected("Failed to resolve ignore rule base folder: " +
                             root_folder_id_result.error());
    }

    auto root_folder_result =
        Folder::Repository::get_folder_by_id(app_state, root_folder_id_result.value());
    if (!root_folder_result) {
      return std::unexpected("Failed to load ignore rule base folder: " +
                             root_folder_result.error());
    }
    if (!root_folder_result->has_value()) {
      return std::unexpected("Ignore rule base folder not found: " +
                             std::to_string(root_folder_id_result.value()));
    }

    auto ignore_base_path = std::filesystem::path(root_folder_result->value().path);
    auto rules_result = Ignore::Service::load_ignore_rules(app_state, folder_id);
    if (!rules_result) {
      return std::unexpected("Failed to load ignore rules: " + rules_result.error());
    }

    auto combined_rules = std::move(rules_result.value());
    auto supported_extensions =
        options.supported_extensions.value_or(Common::default_supported_extensions());
    auto normalized_scan_root_result = Utils::Path::NormalizePath(directory);
    if (!normalized_scan_root_result) {
      return std::unexpected("Failed to normalize scan root: " +
                             normalized_scan_root_result.error());
    }
    auto normalized_scan_root = normalized_scan_root_result.value();

    DiscoveredPaths result;
    // 监听根是目录树的稳定起点，不受根内相对规则影响。
    append_folder_with_ancestors(result, normalized_scan_root, normalized_scan_root);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
      bool is_directory = false;
      bool is_regular_file = false;
      try {
        is_directory = entry.is_directory();
        is_regular_file = entry.is_regular_file();
      } catch (const std::filesystem::filesystem_error& e) {
        Logger().warn("Skipping path due to access error: {}", e.what());
        continue;
      }

      auto normalized_path_result = Utils::Path::NormalizePath(entry.path());
      if (!normalized_path_result) {
        Logger().warn("Failed to normalize discovered path '{}': {}", entry.path().string(),
                      normalized_path_result.error());
        continue;
      }
      auto normalized_path = normalized_path_result.value();

      if (is_directory) {
        // 只把目录本身未被排除的节点加入库存，但不剪枝，以便后续 include 重新放行深层路径。
        if (!Ignore::Service::apply_ignore_rules(normalized_path, ignore_base_path, combined_rules,
                                                 true)) {
          append_folder_with_ancestors(result, normalized_path, normalized_scan_root);
        }
        continue;
      }

      if (!is_regular_file || !Common::is_supported_file(normalized_path, supported_extensions)) {
        continue;
      }

      // 文件继续沿用现有后规则覆盖前规则的 include/exclude 语义。
      if (Ignore::Service::apply_ignore_rules(normalized_path, ignore_base_path, combined_rules,
                                              false)) {
        continue;
      }

      result.files.push_back(normalized_path);
      // 被放行的深层文件必须拥有完整父链，即使某个祖先自身命中了默认 exclude。
      append_folder_with_ancestors(result, normalized_path.parent_path(), normalized_scan_root);
    }

    return result;
  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected("Filesystem error: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return std::unexpected("Exception in scan_paths: " + std::string(e.what()));
  }
}

// 为每个候选文件读取 size / mtime / ctime，供后续变更判定使用。
auto scan_file_info(const std::vector<std::filesystem::path>& found_files)
    -> std::vector<Types::FileSystemInfo> {
  std::vector<Types::FileSystemInfo> result;
  result.reserve(found_files.size());

  for (const auto& file_path : found_files) {
    std::error_code ec;
    auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec) {
      continue;
    }

    auto last_write_time = std::filesystem::last_write_time(file_path, ec);
    if (ec) {
      continue;
    }

    auto creation_time_result = Utils::Time::get_file_creation_time_millis(file_path);
    if (!creation_time_result) {
      Logger().debug("Could not get creation time for {}: {}", file_path.string(),
                     creation_time_result.error());
      continue;
    }

    result.push_back(Types::FileSystemInfo{
        .path = file_path,
        .size = static_cast<std::int64_t>(file_size),
        .file_modified_millis = Utils::Time::file_time_to_millis(last_write_time),
        .file_created_millis = creation_time_result.value(),
        .hash = ""});
  }

  return result;
}

// 发现阶段：一次枚举产出未忽略的目录库存和候选媒体信息。
auto run_discovery_phase(Core::State::AppState& app_state, const std::filesystem::path& directory,
                         std::int64_t folder_id, const Types::ScanOptions& options,
                         const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> std::expected<DiscoveryResult, std::string> {
  Progress::report_scan_progress(progress_callback, "discovering", 0, 1,
                                 Progress::kDiscoveringStartPercent,
                                 "Scanning files and folders from disk");

  auto paths_result = scan_paths(app_state, directory, options, folder_id);
  if (!paths_result) {
    return std::unexpected("Failed to scan directory " + directory.string() + ": " +
                           paths_result.error());
  }

  auto file_infos = scan_file_info(paths_result->files);
  auto folder_paths = std::move(paths_result->folders);
  Progress::report_scan_progress(
      progress_callback, "discovering", static_cast<std::int64_t>(file_infos.size()),
      static_cast<std::int64_t>(file_infos.size()), Progress::kDiscoveringEndPercent,
      std::format("Discovered {} candidate files and {} folders", file_infos.size(),
                  folder_paths.size()));

  Logger().info("Scanned {} files and {} folders in directory '{}' after ignore rules",
                file_infos.size(), folder_paths.size(), directory.string());
  return DiscoveryResult{
      .file_infos = std::move(file_infos),
      .folder_paths = std::move(folder_paths),
  };
}

}  // namespace Features::Gallery::Scanner::Discovery
