#include "features/gallery/folder/repository.hpp"

#include "vendor/std.hpp"

#include "vendor/rfl.hpp"

#include "core/database/database.hpp"
#include "core/database/types.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"

namespace features::gallery::folder::repository {

auto create_folder(core::AppState& app_state, const Folder& folder)
    -> std::expected<std::int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO folders (
                path, parent_id, name, display_name, 
                sort_order, is_hidden
            ) VALUES (?, ?, ?, ?, ?, ?)
            RETURNING id
        )";

  std::vector<core::database::DbParam> params;
  params.push_back(folder.path);

  params.push_back(folder.parent_id.has_value() ? core::database::DbParam{folder.parent_id.value()}
                                                : core::database::DbParam{std::monostate{}});

  params.push_back(folder.name);

  params.push_back(folder.display_name.has_value()
                       ? core::database::DbParam{folder.display_name.value()}
                       : core::database::DbParam{std::monostate{}});

  params.push_back(static_cast<int64_t>(folder.sort_order));
  params.push_back(folder.is_hidden);

  auto result = core::database::query_scalar<std::int64_t>(app_state, sql, params);
  if (!result || !result->has_value()) {
    return std::unexpected("Failed to insert folder: " +
                           (result ? std::string("missing returned ID") : result.error()));
  }

  return result->value();
}

auto get_folder_by_path(core::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE path = ?
        )";

  std::vector<core::database::DbParam> params = {path};

  auto result = core::database::query_single<Folder>(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to query folder by path: " + result.error());
  }

  return result.value();
}

auto get_folder_by_id(core::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE id = ?
        )";

  std::vector<core::database::DbParam> params = {id};

  auto result = core::database::query_single<Folder>(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to query folder by id: " + result.error());
  }

  return result.value();
}

auto update_folder(core::AppState& app_state, const Folder& folder)
    -> std::expected<void, std::string> {
  std::string sql = R"(
            UPDATE folders SET
                path = ?, parent_id = ?, name = ?, display_name = ?,
                cover_asset_id = ?, sort_order = ?, is_hidden = ?
            WHERE id = ?
        )";

  std::vector<core::database::DbParam> params;
  params.push_back(folder.path);

  params.push_back(folder.parent_id.has_value() ? core::database::DbParam{folder.parent_id.value()}
                                                : core::database::DbParam{std::monostate{}});

  params.push_back(folder.name);

  params.push_back(folder.display_name.has_value()
                       ? core::database::DbParam{folder.display_name.value()}
                       : core::database::DbParam{std::monostate{}});

  params.push_back(folder.cover_asset_id.has_value()
                       ? core::database::DbParam{folder.cover_asset_id.value()}
                       : core::database::DbParam{std::monostate{}});

  params.push_back(static_cast<int64_t>(folder.sort_order));
  params.push_back(folder.is_hidden);
  params.push_back(folder.id);

  auto result = core::database::execute(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to update folder: " + result.error());
  }

  return {};
}

// 仅为空名称执行条件更新，避免异步自动命名覆盖用户刚保存的名称。
auto update_folder_display_name_if_empty(core::AppState& app_state, std::int64_t folder_id,
                                         const std::string& display_name)
    -> std::expected<bool, std::string> {
  auto result = core::database::query_scalar<std::int64_t>(app_state,
                                                           R"(
        UPDATE folders
        SET display_name = ?
        WHERE id = ?
          AND (display_name IS NULL OR TRIM(display_name) = '')
        RETURNING id
      )",
                                                           {display_name, folder_id});
  if (!result) {
    return std::unexpected("Failed to conditionally update folder display name: " + result.error());
  }

  // RETURNING 有结果才表示名称在本次调用中由空值变为自动名称。
  return result->has_value();
}

auto delete_folder(core::AppState& app_state, std::int64_t id) -> std::expected<void, std::string> {
  // 暂时实现硬删除，实际项目中可能需要考虑级联删除等问题
  std::string sql = "DELETE FROM folders WHERE id = ?";
  std::vector<core::database::DbParam> params = {id};

  auto result = core::database::execute(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to delete folder: " + result.error());
  }

  return {};
}

// 一次读取指定扫描根及其全部子目录，避免全量扫描反复查询单个路径。
auto list_folders_under_root(core::AppState& app_state, const std::string& root_path)
    -> std::expected<std::vector<Folder>, std::string> {
  auto descendant_begin = root_path;
  if (!descendant_begin.ends_with('/')) {
    descendant_begin.push_back('/');
  }
  // 内部路径统一使用正斜杠；把末尾 '/' 推进为 '0'，形成可走 path 索引的精确前缀区间。
  auto descendant_end = descendant_begin;
  descendant_end.back() = static_cast<char>(descendant_end.back() + 1);

  auto result = core::database::query<Folder>(app_state,
                                              R"(
        SELECT id, path, parent_id, name, display_name,
               cover_asset_id, sort_order, is_hidden,
               created_at, updated_at
        FROM folders
        WHERE path = ? OR (path >= ? AND path < ?)
        ORDER BY path
      )",
                                              {root_path, descendant_begin, descendant_end});
  if (!result) {
    return std::unexpected("Failed to list folders under root: " + result.error());
  }
  return result.value();
}

// 在一个事务中按调用方给出的子先父后顺序删除目录。
auto batch_delete_folders_by_ids(core::AppState& app_state,
                                 const std::vector<std::int64_t>& folder_ids)
    -> std::expected<void, std::string> {
  if (folder_ids.empty()) {
    return {};
  }

  return core::database::execute_transaction(
      app_state, [&folder_ids](core::AppState& txn_app_state) -> std::expected<void, std::string> {
        for (const auto folder_id : folder_ids) {
          auto delete_result = core::database::execute(
              txn_app_state, "DELETE FROM folders WHERE id = ?", {folder_id});
          if (!delete_result) {
            return std::unexpected("Failed to delete folder (id=" + std::to_string(folder_id) +
                                   "): " + delete_result.error());
          }
        }
        return {};
      });
}

auto list_all_folders(core::AppState& app_state)
    -> std::expected<std::vector<Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            ORDER BY path
        )";

  auto result = core::database::query<Folder>(app_state, sql);
  if (!result) {
    return std::unexpected("Failed to list all folders: " + result.error());
  }

  return result.value();
}

auto get_child_folders(core::AppState& app_state, std::optional<std::int64_t> parent_id)
    -> std::expected<std::vector<Folder>, std::string> {
  std::string sql;
  std::vector<core::database::DbParam> params;

  if (parent_id.has_value()) {
    sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE parent_id = ?
            ORDER BY sort_order, name
        )";
    params.push_back(parent_id.value());
  } else {
    // 获取根文件夹（parent_id 为 NULL）
    sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE parent_id IS NULL
            ORDER BY sort_order, name
        )";
  }

  auto result = core::database::query<Folder>(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to get child folders: " + result.error());
  }

  return result.value();
}

auto get_folder_tree(core::AppState& app_state)
    -> std::expected<std::vector<FolderTreeNode>, std::string> {
  // 1. 获取所有文件夹
  auto folders_result = list_all_folders(app_state);
  if (!folders_result) {
    return std::unexpected("Failed to get all folders: " + folders_result.error());
  }

  const auto& folders = folders_result.value();

  // 2. 查询每个文件夹的直接 assets 数量
  std::unordered_map<std::int64_t, std::int64_t> direct_asset_counts;
  std::string count_sql = R"(
            SELECT folder_id, COUNT(*) as count
            FROM assets
            WHERE folder_id IS NOT NULL AND missing_at IS NULL
            GROUP BY folder_id
        )";

  // 定义用于接收统计结果的结构
  struct FolderAssetCount {
    std::int64_t folder_id;
    std::int64_t count;
  };

  auto count_result = core::database::query<FolderAssetCount>(app_state, count_sql);
  if (!count_result) {
    return std::unexpected("Failed to query asset counts: " + count_result.error());
  }

  // 填充直接 assets 数量映射
  for (const auto& item : count_result.value()) {
    direct_asset_counts[item.folder_id] = item.count;
  }

  // 2. 创建 id -> FolderTreeNode 的映射，用于快速查找
  std::unordered_map<std::int64_t, FolderTreeNode> node_map;

  // 第一次遍历：创建所有节点
  for (const auto& folder : folders) {
    FolderTreeNode node{.id = folder.id,
                        .path = folder.path,
                        .parent_id = folder.parent_id,
                        .name = folder.name,
                        .display_name = folder.display_name,
                        .cover_asset_id = folder.cover_asset_id,
                        .sort_order = folder.sort_order,
                        .is_hidden = folder.is_hidden,
                        .created_at = folder.created_at,
                        .updated_at = folder.updated_at,
                        .is_network = utils::path::ClassifyPathStorageKind(std::filesystem::path(
                                          folder.path)) == utils::path::PathStorageKind::RemoteUnc,
                        .children = {}};

    node_map[folder.id] = std::move(node);
  }

  // 3. 第二次遍历：构建父子关系（收集子节点ID）
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> parent_to_children;
  std::vector<std::int64_t> root_ids;

  for (const auto& folder : folders) {
    if (folder.parent_id.has_value()) {
      // 有父节点，记录到父节点的子节点列表中
      parent_to_children[folder.parent_id.value()].push_back(folder.id);
    } else {
      // 没有父节点，是根节点
      root_ids.push_back(folder.id);
    }
  }

  // 4. 递归构建树结构
  auto build_tree = [&](this auto&& self, std::int64_t folder_id) -> FolderTreeNode {
    auto node_it = node_map.find(folder_id);
    if (node_it == node_map.end()) {
      Logger().error("Folder {} not found in node_map", folder_id);
      return FolderTreeNode{};
    }

    FolderTreeNode node = std::move(node_it->second);

    // 递归构建子节点
    auto children_it = parent_to_children.find(folder_id);
    if (children_it != parent_to_children.end()) {
      for (std::int64_t child_id : children_it->second) {
        node.children.push_back(self(child_id));
      }
    }

    return node;
  };

  // 5. 构建所有根节点
  std::vector<FolderTreeNode> root_nodes;
  for (std::int64_t root_id : root_ids) {
    root_nodes.push_back(build_tree(root_id));
  }

  // 6. 对根节点按 sort_order 和 name 排序
  std::sort(root_nodes.begin(), root_nodes.end(),
            [](const FolderTreeNode& a, const FolderTreeNode& b) {
              if (a.sort_order != b.sort_order) {
                return a.sort_order < b.sort_order;
              }
              return a.name < b.name;
            });

  // 递归排序所有子节点
  auto sort_children = [&](this auto&& self, FolderTreeNode& node) -> void {
    std::sort(node.children.begin(), node.children.end(),
              [](const FolderTreeNode& a, const FolderTreeNode& b) {
                if (a.sort_order != b.sort_order) {
                  return a.sort_order < b.sort_order;
                }
                return a.name < b.name;
              });

    for (auto& child : node.children) {
      self(child);
    }
  };

  for (auto& root : root_nodes) {
    sort_children(root);
  }

  // 7. 递归计算每个文件夹的 asset_count（包含所有子文件夹）
  auto calculate_total_assets = [&](this auto&& self, FolderTreeNode& node) -> std::int64_t {
    // 当前文件夹的直接 assets 数量
    std::int64_t total = 0;
    auto it = direct_asset_counts.find(node.id);
    if (it != direct_asset_counts.end()) {
      total = it->second;
    }

    // 递归累加所有子文件夹的 assets
    for (auto& child : node.children) {
      total += self(child);
    }

    // 设置节点的 asset_count
    node.asset_count = total;
    return total;
  };

  // 对所有根节点执行计算
  for (auto& root : root_nodes) {
    calculate_total_assets(root);
  }

  return root_nodes;
}

}  // namespace features::gallery::folder::repository
