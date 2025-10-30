module;

module Features.Gallery.Folder.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.Types;
import Utils.Logger;
import <rfl.hpp>;

namespace Features::Gallery::Folder::Repository {

auto create_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<std::int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO folders (
                path, parent_id, name, display_name, 
                sort_order, is_hidden
            ) VALUES (?, ?, ?, ?, ?, ?)
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(folder.path);

  params.push_back(folder.parent_id.has_value()
                       ? Core::Database::Types::DbParam{folder.parent_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(folder.name);

  params.push_back(folder.display_name.has_value()
                       ? Core::Database::Types::DbParam{folder.display_name.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(static_cast<int64_t>(folder.sort_order));
  params.push_back(folder.is_hidden);

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to insert folder: " + result.error());
  }

  // 获取插入的 ID
  auto id_result =
      Core::Database::query_scalar<int64_t>(*app_state.database, "SELECT last_insert_rowid()");
  if (!id_result) {
    return std::unexpected("Failed to get inserted folder ID: " + id_result.error());
  }

  return id_result->value_or(0);
}

auto get_folder_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE path = ?
        )";

  std::vector<Core::Database::Types::DbParam> params = {path};

  auto result = Core::Database::query_single<Types::Folder>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query folder by path: " + result.error());
  }

  return result.value();
}

auto get_folder_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE id = ?
        )";

  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::query_single<Types::Folder>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query folder by id: " + result.error());
  }

  return result.value();
}

auto update_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<void, std::string> {
  std::string sql = R"(
            UPDATE folders SET
                path = ?, parent_id = ?, name = ?, display_name = ?,
                cover_asset_id = ?, sort_order = ?, is_hidden = ?
            WHERE id = ?
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(folder.path);

  params.push_back(folder.parent_id.has_value()
                       ? Core::Database::Types::DbParam{folder.parent_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(folder.name);

  params.push_back(folder.display_name.has_value()
                       ? Core::Database::Types::DbParam{folder.display_name.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(folder.cover_asset_id.has_value()
                       ? Core::Database::Types::DbParam{folder.cover_asset_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(static_cast<int64_t>(folder.sort_order));
  params.push_back(folder.is_hidden);
  params.push_back(folder.id);

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to update folder: " + result.error());
  }

  return {};
}

auto delete_folder(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string> {
  // 暂时实现硬删除，实际项目中可能需要考虑级联删除等问题
  std::string sql = "DELETE FROM folders WHERE id = ?";
  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to delete folder: " + result.error());
  }

  return {};
}

auto list_all_folders(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            ORDER BY path
        )";

  auto result = Core::Database::query<Types::Folder>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to list all folders: " + result.error());
  }

  return result.value();
}

auto get_child_folders(Core::State::AppState& app_state, std::optional<std::int64_t> parent_id)
    -> std::expected<std::vector<Types::Folder>, std::string> {
  std::string sql;
  std::vector<Core::Database::Types::DbParam> params;

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

  auto result = Core::Database::query<Types::Folder>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to get child folders: " + result.error());
  }

  return result.value();
}

auto get_folder_tree(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::FolderTreeNode>, std::string> {
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
            WHERE folder_id IS NOT NULL
            GROUP BY folder_id
        )";

  // 定义用于接收统计结果的结构
  struct FolderAssetCount {
    std::int64_t folder_id;
    std::int64_t count;
  };

  auto count_result = Core::Database::query<FolderAssetCount>(*app_state.database, count_sql);
  if (!count_result) {
    return std::unexpected("Failed to query asset counts: " + count_result.error());
  }

  // 填充直接 assets 数量映射
  for (const auto& item : count_result.value()) {
    direct_asset_counts[item.folder_id] = item.count;
  }

  // 2. 创建 id -> FolderTreeNode 的映射，用于快速查找
  std::unordered_map<std::int64_t, Types::FolderTreeNode> node_map;

  // 第一次遍历：创建所有节点
  for (const auto& folder : folders) {
    Types::FolderTreeNode node{.id = folder.id,
                               .path = folder.path,
                               .parent_id = folder.parent_id,
                               .name = folder.name,
                               .display_name = folder.display_name,
                               .cover_asset_id = folder.cover_asset_id,
                               .sort_order = folder.sort_order,
                               .is_hidden = folder.is_hidden,
                               .created_at = folder.created_at,
                               .updated_at = folder.updated_at,
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
  std::function<Types::FolderTreeNode(std::int64_t)> build_tree;
  build_tree = [&](std::int64_t folder_id) -> Types::FolderTreeNode {
    auto node_it = node_map.find(folder_id);
    if (node_it == node_map.end()) {
      Logger().error("Folder {} not found in node_map", folder_id);
      return Types::FolderTreeNode{};
    }

    Types::FolderTreeNode node = std::move(node_it->second);

    // 递归构建子节点
    auto children_it = parent_to_children.find(folder_id);
    if (children_it != parent_to_children.end()) {
      for (std::int64_t child_id : children_it->second) {
        node.children.push_back(build_tree(child_id));
      }
    }

    return node;
  };

  // 5. 构建所有根节点
  std::vector<Types::FolderTreeNode> root_nodes;
  for (std::int64_t root_id : root_ids) {
    root_nodes.push_back(build_tree(root_id));
  }

  // 6. 对根节点按 sort_order 和 name 排序
  std::sort(root_nodes.begin(), root_nodes.end(),
            [](const Types::FolderTreeNode& a, const Types::FolderTreeNode& b) {
              if (a.sort_order != b.sort_order) {
                return a.sort_order < b.sort_order;
              }
              return a.name < b.name;
            });

  // 递归排序所有子节点
  std::function<void(Types::FolderTreeNode&)> sort_children;
  sort_children = [&](Types::FolderTreeNode& node) {
    std::sort(node.children.begin(), node.children.end(),
              [](const Types::FolderTreeNode& a, const Types::FolderTreeNode& b) {
                if (a.sort_order != b.sort_order) {
                  return a.sort_order < b.sort_order;
                }
                return a.name < b.name;
              });

    for (auto& child : node.children) {
      sort_children(child);
    }
  };

  for (auto& root : root_nodes) {
    sort_children(root);
  }

  // 7. 递归计算每个文件夹的 asset_count（包含所有子文件夹）
  std::function<std::int64_t(Types::FolderTreeNode&)> calculate_total_assets;
  calculate_total_assets = [&](Types::FolderTreeNode& node) -> std::int64_t {
    // 当前文件夹的直接 assets 数量
    std::int64_t total = 0;
    auto it = direct_asset_counts.find(node.id);
    if (it != direct_asset_counts.end()) {
      total = it->second;
    }

    // 递归累加所有子文件夹的 assets
    for (auto& child : node.children) {
      total += calculate_total_assets(child);
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

}  // namespace Features::Gallery::Folder::Repository
