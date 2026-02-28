module;

module Features.Gallery.Folder.Service;

import std;
import Core.State;
import Core.Database;
import Features.Gallery.Types;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Watcher;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Utils.System;

namespace Features::Gallery::Folder::Service {

// ============= 路径处理辅助函数 =============

auto extract_unique_folder_paths(const std::vector<std::filesystem::path>& file_paths,
                                 const std::filesystem::path& scan_root)
    -> std::vector<std::filesystem::path> {
  std::unordered_set<std::string> unique_paths;
  std::vector<std::filesystem::path> result;

  // 规范化扫描根目录
  auto normalized_scan_root_result = Utils::Path::NormalizePath(scan_root);
  if (!normalized_scan_root_result) {
    Logger().error("Failed to normalize scan root path '{}': {}", scan_root.string(),
                   normalized_scan_root_result.error());
    return result;
  }
  auto normalized_scan_root = normalized_scan_root_result.value();
  std::string scan_root_str = normalized_scan_root.string();

  for (const auto& file_path : file_paths) {
    auto current_path = file_path.parent_path();

    // 从文件的父目录开始，递归向上直到扫描根目录
    while (!current_path.empty()) {
      auto normalized_result = Utils::Path::NormalizePath(current_path);
      if (!normalized_result) {
        Logger().warn("Failed to normalize path '{}': {}", current_path.string(),
                      normalized_result.error());
        break;
      }

      auto normalized = normalized_result.value();
      std::string path_str = normalized.string();

      // 如果已经到达或超出扫描根目录，停止
      if (path_str == scan_root_str) {
        break;
      }

      // 检查是否是扫描根目录的子路径
      if (path_str.size() < scan_root_str.size() ||
          path_str.substr(0, scan_root_str.size()) != scan_root_str) {
        // 已经超出扫描根目录范围，停止
        break;
      }

      // 添加到结果集
      if (unique_paths.insert(path_str).second) {
        result.push_back(normalized);
      }

      // 继续向上遍历
      current_path = current_path.parent_path();
    }
  }

  // 确保根目录本身也在结果中，以便子文件夹能找到父ID
  if (unique_paths.insert(scan_root_str).second) {
    result.push_back(normalized_scan_root);
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

// 批量创建文件夹记录（统一的文件夹创建接口）
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

    // 如果已经处理过，跳过
    if (path_to_id_map.contains(path_str)) {
      continue;
    }

    // 首先检查数据库中是否已存在
    auto existing_folder_result = Repository::get_folder_by_path(app_state, path_str);
    if (!existing_folder_result) {
      Logger().error("Failed to query folder for path '{}': {}", path_str,
                     existing_folder_result.error());
      continue;
    }

    if (existing_folder_result->has_value()) {
      // 文件夹已存在，直接使用
      auto folder_id = existing_folder_result->value().id;
      path_to_id_map[path_str] = folder_id;
      Logger().debug("Found existing folder '{}' with ID {}", path_str, folder_id);
      continue;
    }

    // 文件夹不存在，需要创建
    // 计算父路径和 parent_id
    std::optional<std::int64_t> parent_id;
    auto parent_path = folder_path.parent_path();

    if (!parent_path.empty() && parent_path != folder_path.root_path()) {
      std::string parent_path_str = parent_path.string();

      // 由于已经按深度排序，父目录的 ID 应该已经在 map 中
      if (auto it = path_to_id_map.find(parent_path_str); it != path_to_id_map.end()) {
        parent_id = it->second;
      } else {
        Logger().info("Parent folder '{}' not found in map for child '{}'", parent_path_str,
                      path_str);
      }
    }

    // 创建新文件夹
    std::string folder_name = folder_path.filename().string();
    Types::Folder new_folder{.path = path_str, .parent_id = parent_id, .name = folder_name};

    auto create_result = Repository::create_folder(app_state, new_folder);
    if (!create_result) {
      Logger().error("Failed to create folder for path '{}': {}", path_str, create_result.error());
      continue;
    }

    auto folder_id = create_result.value();
    path_to_id_map[path_str] = folder_id;
    Logger().debug("Created folder '{}' with ID {} (parent_id: {})", path_str, folder_id,
                   parent_id.has_value() ? std::to_string(parent_id.value()) : "NULL");
  }

  return path_to_id_map;
}

auto normalize_display_name(const std::optional<std::string>& display_name)
    -> std::optional<std::string> {
  if (!display_name.has_value()) {
    return std::nullopt;
  }

  auto trimmed = Utils::String::TrimAscii(display_name.value());
  if (trimmed.empty()) {
    return std::nullopt;
  }

  return trimmed;
}

auto cleanup_root_folder_index(Core::State::AppState& app_state, std::int64_t root_folder_id,
                               const std::string& root_path)
    -> std::expected<std::int64_t, std::string> {
  return Core::Database::execute_transaction(
      *app_state.database, [&](auto& db_state) -> std::expected<std::int64_t, std::string> {
        auto delete_assets_result =
            Core::Database::execute(db_state, "DELETE FROM assets WHERE path = ? OR path LIKE ?",
                                    {root_path, root_path + "/%"});
        if (!delete_assets_result) {
          return std::unexpected("Failed to delete assets under root path: " +
                                 delete_assets_result.error());
        }

        auto deleted_assets_result =
            Core::Database::query_scalar<std::int64_t>(db_state, "SELECT changes()");
        if (!deleted_assets_result) {
          return std::unexpected("Failed to query deleted assets count: " +
                                 deleted_assets_result.error());
        }
        auto deleted_assets = deleted_assets_result->value_or(0);

        std::string delete_folders_sql = R"(
          DELETE FROM folders
          WHERE id IN (
            WITH RECURSIVE folder_tree(id) AS (
              SELECT id FROM folders WHERE id = ?
              UNION ALL
              SELECT f.id FROM folders f
              INNER JOIN folder_tree t ON f.parent_id = t.id
            )
            SELECT id FROM folder_tree
          )
        )";

        auto delete_folders_result =
            Core::Database::execute(db_state, delete_folders_sql, {root_folder_id});
        if (!delete_folders_result) {
          return std::unexpected("Failed to delete folders under root: " +
                                 delete_folders_result.error());
        }

        auto deleted_folders_result =
            Core::Database::query_scalar<std::int64_t>(db_state, "SELECT changes()");
        if (!deleted_folders_result) {
          return std::unexpected("Failed to query deleted folders count: " +
                                 deleted_folders_result.error());
        }
        auto deleted_folders = deleted_folders_result->value_or(0);

        return deleted_assets + deleted_folders;
      });
}

auto update_folder_display_name(Core::State::AppState& app_state, std::int64_t folder_id,
                                const std::optional<std::string>& display_name)
    -> std::expected<Types::OperationResult, std::string> {
  auto folder_result = Repository::get_folder_by_id(app_state, folder_id);
  if (!folder_result) {
    return std::unexpected("Failed to query folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return std::unexpected("Folder not found: " + std::to_string(folder_id));
  }

  auto folder = folder_result->value();
  folder.display_name = normalize_display_name(display_name);

  auto update_result = Repository::update_folder(app_state, folder);
  if (!update_result) {
    return std::unexpected("Failed to update folder: " + update_result.error());
  }

  return Types::OperationResult{
      .success = true,
      .message = folder.display_name.has_value() ? "Folder display name updated"
                                                 : "Folder display name reset",
      .affected_count = 1,
  };
}

auto open_folder_in_explorer(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<Types::OperationResult, std::string> {
  auto folder_result = Repository::get_folder_by_id(app_state, folder_id);
  if (!folder_result) {
    return std::unexpected("Failed to query folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return std::unexpected("Folder not found: " + std::to_string(folder_id));
  }

  auto open_result =
      Utils::System::open_directory(std::filesystem::path(folder_result->value().path));
  if (!open_result) {
    return std::unexpected("Failed to open folder in explorer: " + open_result.error());
  }

  return Types::OperationResult{
      .success = true,
      .message = "Folder opened in explorer",
      .affected_count = 0,
  };
}

auto remove_root_folder_watch(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<Types::OperationResult, std::string> {
  auto folder_result = Repository::get_folder_by_id(app_state, folder_id);
  if (!folder_result) {
    return std::unexpected("Failed to query folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return std::unexpected("Folder not found: " + std::to_string(folder_id));
  }

  const auto& folder = folder_result->value();
  if (folder.parent_id.has_value()) {
    return std::unexpected("Only root folders can be removed from watch list");
  }

  auto remove_watcher_result = Features::Gallery::Watcher::remove_watcher_for_directory(
      app_state, std::filesystem::path(folder.path));
  if (!remove_watcher_result) {
    return std::unexpected("Failed to remove watcher: " + remove_watcher_result.error());
  }

  auto cleanup_result = cleanup_root_folder_index(app_state, folder.id, folder.path);
  if (!cleanup_result) {
    return std::unexpected("Failed to cleanup folder index: " + cleanup_result.error());
  }

  return Types::OperationResult{
      .success = true,
      .message = "Folder removed from watch list and index cleaned",
      .affected_count = cleanup_result.value(),
  };
}

}  // namespace Features::Gallery::Folder::Service
