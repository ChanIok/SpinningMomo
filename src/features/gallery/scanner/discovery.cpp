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

// 递归枚举目录：常规文件 → 扩展名 → ignore 规则
auto scan_paths(Core::State::AppState& app_state, const std::filesystem::path& directory,
                const Types::ScanOptions& options, std::int64_t folder_id)
    -> std::expected<std::vector<std::filesystem::path>, std::string> {
  if (!std::filesystem::exists(directory)) {
    return std::unexpected("Directory does not exist: " + directory.string());
  }

  if (!std::filesystem::is_directory(directory)) {
    return std::unexpected("Path is not a directory: " + directory.string());
  }

  try {
    // 扫描范围可以是子目录，但规则始终以其所属的顶层监听目录为匹配基准。
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

    // 惰性 ranges 管道：过滤后才收集为 vector
    auto found_files =
        std::ranges::subrange(std::filesystem::recursive_directory_iterator(directory),
                              std::filesystem::recursive_directory_iterator{}) |
        // 只保留常规文件，访问失败则跳过
        std::views::filter([](const std::filesystem::directory_entry& entry) {
          try {
            return entry.is_regular_file();
          } catch (const std::filesystem::filesystem_error& e) {
            Logger().warn("Skipping file due to access error: {}", e.what());
            return false;
          }
        }) |
        // 扩展名白名单
        std::views::filter([&supported_extensions](const std::filesystem::directory_entry& entry) {
          return Common::is_supported_file(entry.path(), supported_extensions);
        }) |
        // ignore 规则（相对顶层 root 匹配）
        std::views::filter([&ignore_base_path,
                            &combined_rules](const std::filesystem::directory_entry& entry) {
          bool should_ignore =
              Ignore::Service::apply_ignore_rules(entry.path(), ignore_base_path, combined_rules);
          return !should_ignore;
        }) |
        std::views::transform(
            [](const std::filesystem::directory_entry& entry) { return entry.path(); }) |
        std::ranges::to<std::vector>();

    return found_files;

  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected("Filesystem error: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return std::unexpected("Exception in scan_paths: " + std::string(e.what()));
  }
}

// 为每个候选路径读取 size / mtime / ctime，并规范化路径
auto scan_file_info(Core::State::AppState& app_state, const std::filesystem::path& directory,
                    const Types::ScanOptions& options, std::int64_t folder_id)
    -> std::expected<std::vector<Types::FileSystemInfo>, std::string> {
  auto files_result = scan_paths(app_state, directory, options, folder_id);
  if (!files_result) {
    return std::unexpected("Failed to scan directory " + directory.string() + ": " +
                           files_result.error());
  }

  auto found_files = std::move(files_result.value());
  std::vector<Types::FileSystemInfo> result;
  result.reserve(found_files.size());

  for (const auto& file_path : found_files) {
    std::error_code ec;
    auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec) continue;

    auto last_write_time = std::filesystem::last_write_time(file_path, ec);
    if (ec) continue;

    auto creation_time_result = Utils::Time::get_file_creation_time_millis(file_path);
    if (!creation_time_result) {
      Logger().debug("Could not get creation time for {}: {}", file_path.string(),
                     creation_time_result.error());
      continue;
    }

    // 统一路径格式（正斜杠），供后续 folder / cache / cleanup 使用
    auto normalized_path_result = Utils::Path::NormalizePath(file_path);
    if (!normalized_path_result) {
      Logger().warn("Failed to normalize path '{}': {}", file_path.string(),
                    normalized_path_result.error());
      continue;
    }

    Types::FileSystemInfo info{
        .path = normalized_path_result.value(),
        .size = static_cast<std::int64_t>(file_size),
        .file_modified_millis = Utils::Time::file_time_to_millis(last_write_time),
        .file_created_millis = creation_time_result.value(),
        .hash = ""};

    result.push_back(std::move(info));
  }

  Logger().info("Scanned {} files with ignore rules applied", result.size());
  return result;
}

// 发现阶段：按扩展名与 ignore 规则枚举磁盘候选文件，并读取 size/mtime/ctime
auto run_discovery_phase(Core::State::AppState& app_state, const std::filesystem::path& directory,
                         std::int64_t folder_id, const Types::ScanOptions& options,
                         const std::function<void(const Types::ScanProgress&)>& progress_callback)
    -> std::expected<std::vector<Types::FileSystemInfo>, std::string> {
  Progress::report_scan_progress(progress_callback, "discovering", 0, 1,
                                 Progress::kDiscoveringStartPercent, "Scanning files from disk");

  auto file_info_result = scan_file_info(app_state, directory, options, folder_id);
  if (!file_info_result) {
    return std::unexpected("File info scanning failed: " + file_info_result.error());
  }

  auto file_infos = std::move(file_info_result.value());
  Progress::report_scan_progress(
      progress_callback, "discovering", static_cast<std::int64_t>(file_infos.size()),
      static_cast<std::int64_t>(file_infos.size()), Progress::kDiscoveringEndPercent,
      std::format("Discovered {} candidate files", file_infos.size()));

  Logger().info("Scanned {} files in directory '{}' (after ignore rules)", file_infos.size(),
                directory.string());
  return file_infos;
}

}  // namespace Features::Gallery::Scanner::Discovery
