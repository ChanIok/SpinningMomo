module;

#include <format>

module Features.Gallery.Folder.Processor;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Folder.Repository;
import Utils.Logger;
import Utils.Path;

namespace Features::Gallery::Folder::Processor {

// ============= 路径处理辅助函数 =============

auto extract_unique_folder_paths(const std::vector<std::filesystem::path>& file_paths)
    -> std::vector<std::filesystem::path> {
  std::unordered_set<std::string> unique_paths;
  std::vector<std::filesystem::path> result;

  for (const auto& file_path : file_paths) {
    auto parent_path = file_path.parent_path();
    if (!parent_path.empty()) {
      auto normalized_result = Utils::Path::NormalizePath(parent_path);
      if (!normalized_result) {
        Logger().warn("Failed to normalize path '{}': {}", parent_path.string(),
                      normalized_result.error());
        continue;
      }
      auto normalized = normalized_result.value();
      auto path_str = normalized.string();

      if (unique_paths.insert(path_str).second) {
        result.push_back(normalized);
      }
    }
  }

  return result;
}

auto build_folder_hierarchy(const std::vector<std::filesystem::path>& paths)
    -> std::vector<Types::FolderHierarchy> {
  std::vector<Types::FolderHierarchy> result;
  result.reserve(paths.size());

  for (const auto& path : paths) {
    Types::FolderHierarchy hierarchy;
    hierarchy.path = path.string();
    hierarchy.name = path.filename().string();

    // 计算父路径
    auto parent = path.parent_path();
    if (!parent.empty() && parent != path.root_path()) {
      hierarchy.parent_path = parent.string();
    }

    // 计算嵌套层级（简单实现：统计路径分隔符数量）
    std::string path_str = path.string();
    hierarchy.level =
        static_cast<int>(std::ranges::count(path_str, std::filesystem::path::preferred_separator));

    result.push_back(std::move(hierarchy));
  }

  return result;
}

// ============= 文件夹处理核心功能 =============

auto get_or_create_folder_for_file_path(Core::State::AppState& app_state,
                                        const std::filesystem::path& file_path)
    -> std::expected<std::int64_t, std::string> {
  auto parent_path = file_path.parent_path();
  if (parent_path.empty()) {
    return std::unexpected("File has no parent directory: " + file_path.string());
  }

  // 规范化路径
  auto normalized_result = Utils::Path::NormalizePath(parent_path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize path: " + normalized_result.error());
  }
  auto normalized_path = normalized_result.value();

  // 使用Repository的get_or_create功能
  auto folder_id_result =
      Repository::get_or_create_folder_for_path(app_state, normalized_path.string());
  if (!folder_id_result) {
    return std::unexpected("Failed to get or create folder for path: " + folder_id_result.error());
  }

  return folder_id_result.value();
}

auto batch_create_folders_for_paths(Core::State::AppState& app_state,
                                    const std::vector<std::filesystem::path>& folder_paths)
    -> std::expected<std::unordered_map<std::string, std::int64_t>, std::string> {
  std::unordered_map<std::string, std::int64_t> path_to_id_map;

  // 按路径深度排序，确保父目录先创建
  auto sorted_paths = folder_paths;
  std::ranges::sort(sorted_paths, [](const auto& a, const auto& b) {
    // 按路径字符串长度排序（通常深度较浅的路径更短）
    auto a_str = a.string();
    auto b_str = b.string();
    if (a_str.length() != b_str.length()) {
      return a_str.length() < b_str.length();
    }
    return a_str < b_str;  // 相同长度时按字典序
  });

  for (const auto& folder_path : sorted_paths) {
    auto path_str = folder_path.string();

    if (path_to_id_map.contains(path_str)) {
      continue;
    }

    auto folder_id_result = Repository::get_or_create_folder_for_path(app_state, path_str);
    if (!folder_id_result) {
      Logger().error("Failed to create folder for path '{}': {}", path_str,
                     folder_id_result.error());
      continue;
    }

    path_to_id_map[path_str] = folder_id_result.value();
    Logger().debug("Created/found folder '{}' with ID {}", path_str, folder_id_result.value());
  }

  return path_to_id_map;
}

 

}  // namespace Features::Gallery::Folder::Processor
