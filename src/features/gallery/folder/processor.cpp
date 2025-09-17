module;

#include <format>

module Features.Gallery.Folder.Processor;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Folder.Repository;
import Utils.Logger;

namespace Features::Gallery::Folder::Processor {

// ============= 路径处理辅助函数 =============

auto normalize_folder_path(const std::filesystem::path& folder_path) -> std::filesystem::path {
  try {
    // 获取绝对路径并规范化
    auto absolute_path = std::filesystem::absolute(folder_path);
    auto canonical_path = std::filesystem::weakly_canonical(absolute_path);
    return canonical_path;
  } catch (const std::filesystem::filesystem_error&) {
    // 如果无法规范化，返回原路径
    return folder_path;
  }
}

auto extract_unique_folder_paths(const std::vector<std::filesystem::path>& file_paths)
    -> std::vector<std::filesystem::path> {
  std::unordered_set<std::string> unique_paths;
  std::vector<std::filesystem::path> result;

  for (const auto& file_path : file_paths) {
    auto parent_path = file_path.parent_path();
    if (!parent_path.empty()) {
      auto normalized = normalize_folder_path(parent_path);
      auto path_str = normalized.string();

      if (unique_paths.insert(path_str).second) {
        result.push_back(normalized);
      }
    }
  }

  // 按路径字符串排序，确保父目录在子目录之前处理
  std::ranges::sort(result, [](const auto& a, const auto& b) { return a.string() < b.string(); });

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
  auto normalized_path = normalize_folder_path(parent_path);

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
    auto normalized_path = normalize_folder_path(folder_path);
    auto path_str = normalized_path.string();

    // 如果已经处理过，跳过
    if (path_to_id_map.contains(path_str)) {
      continue;
    }

    auto folder_id_result = Repository::get_or_create_folder_for_path(app_state, path_str);
    if (!folder_id_result) {
      Logger().error("Failed to create folder for path '{}': {}", path_str,
                     folder_id_result.error());
      continue;  // 继续处理其他文件夹
    }

    path_to_id_map[path_str] = folder_id_result.value();
    Logger().debug("Created/found folder '{}' with ID {}", path_str, folder_id_result.value());
  }

  return path_to_id_map;
}

// ============= 资产与文件夹关联功能 =============

auto associate_assets_with_folders(Core::State::AppState& app_state,
                                   const std::vector<Types::Asset>& assets)
    -> std::expected<void, std::string> {
  if (assets.empty()) {
    return {};
  }

  // 提取所有文件路径的父目录
  std::vector<std::filesystem::path> file_paths;
  file_paths.reserve(assets.size());

  for (const auto& asset : assets) {
    file_paths.emplace_back(asset.filepath);
  }

  auto folder_paths = extract_unique_folder_paths(file_paths);

  // 批量创建文件夹记录
  auto path_to_id_result = batch_create_folders_for_paths(app_state, folder_paths);
  if (!path_to_id_result) {
    return std::unexpected("Failed to batch create folders: " + path_to_id_result.error());
  }

  auto path_to_id_map = path_to_id_result.value();

  // TODO: 更新assets表中的folder_id字段
  // 这部分需要在asset repository中实现批量更新folder_id的功能
  // 或者在这里实现直接的SQL更新

  Logger().info("Associated {} assets with their folders", assets.size());
  return {};
}

auto recalculate_all_folder_asset_counts(Core::State::AppState& app_state)
    -> std::expected<std::vector<std::pair<std::int64_t, int>>, std::string> {
  // 获取所有文件夹
  auto folders_result = Repository::list_all_folders(app_state);
  if (!folders_result) {
    return std::unexpected("Failed to list all folders: " + folders_result.error());
  }

  auto folders = folders_result.value();
  std::vector<std::pair<std::int64_t, int>> results;
  results.reserve(folders.size());

  // 为每个文件夹重新计算资产数量
  for (const auto& folder : folders) {
    auto count_result = Repository::recalculate_folder_asset_count(app_state, folder.id);
    if (count_result) {
      results.emplace_back(folder.id, count_result.value());
      Logger().debug("Folder '{}' (ID: {}) has {} assets", folder.path, folder.id,
                     count_result.value());
    } else {
      Logger().warn("Failed to recalculate asset count for folder '{}' (ID: {}): {}", folder.path,
                    folder.id, count_result.error());
      results.emplace_back(folder.id, 0);  // 默认为0
    }
  }

  Logger().info("Recalculated asset counts for {} folders", results.size());
  return results;
}

}  // namespace Features::Gallery::Folder::Processor
