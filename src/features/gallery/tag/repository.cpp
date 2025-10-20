module;

module Features.Gallery.Tag.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.Types;
import Utils.Logger;

namespace Features::Gallery::Tag::Repository {

// ============= 基本 CRUD 操作 =============

auto create_tag(Core::State::AppState& app_state, const Types::CreateTagParams& params)
    -> std::expected<std::int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO tags (name, parent_id, sort_order)
            VALUES (?, ?, ?)
        )";

  std::vector<Core::Database::Types::DbParam> db_params;
  db_params.push_back(params.name);

  db_params.push_back(params.parent_id.has_value()
                          ? Core::Database::Types::DbParam{params.parent_id.value()}
                          : Core::Database::Types::DbParam{std::monostate{}});

  db_params.push_back(static_cast<int64_t>(params.sort_order.value_or(0)));

  auto result = Core::Database::execute(*app_state.database, sql, db_params);
  if (!result) {
    return std::unexpected("Failed to create tag: " + result.error());
  }

  // 获取插入的 ID
  auto id_result =
      Core::Database::query_scalar<int64_t>(*app_state.database, "SELECT last_insert_rowid()");
  if (!id_result) {
    return std::unexpected("Failed to get inserted tag ID: " + id_result.error());
  }

  return id_result->value_or(0);
}

auto get_tag_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Tag>, std::string> {
  std::string sql = R"(
            SELECT id, name, parent_id, sort_order, created_at, updated_at
            FROM tags
            WHERE id = ?
        )";

  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::query_single<Types::Tag>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query tag by id: " + result.error());
  }

  return result.value();
}

auto get_tag_by_name(Core::State::AppState& app_state, const std::string& name,
                     std::optional<std::int64_t> parent_id)
    -> std::expected<std::optional<Types::Tag>, std::string> {
  std::string sql;
  std::vector<Core::Database::Types::DbParam> params;

  if (parent_id.has_value()) {
    sql = R"(
            SELECT id, name, parent_id, sort_order, created_at, updated_at
            FROM tags
            WHERE name = ? AND parent_id = ?
            LIMIT 1
        )";
    params.push_back(name);
    params.push_back(parent_id.value());
  } else {
    sql = R"(
            SELECT id, name, parent_id, sort_order, created_at, updated_at
            FROM tags
            WHERE name = ? AND parent_id IS NULL
            LIMIT 1
        )";
    params.push_back(name);
  }

  auto result = Core::Database::query_single<Types::Tag>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query tag by name: " + result.error());
  }

  return result.value();
}

auto update_tag(Core::State::AppState& app_state, const Types::UpdateTagParams& params)
    -> std::expected<void, std::string> {
  // 动态构建 UPDATE 语句
  std::vector<std::string> set_clauses;
  std::vector<Core::Database::Types::DbParam> db_params;

  if (params.name.has_value()) {
    set_clauses.push_back("name = ?");
    db_params.push_back(params.name.value());
  }

  if (params.parent_id.has_value()) {
    set_clauses.push_back("parent_id = ?");
    db_params.push_back(params.parent_id.value());
  }

  if (params.sort_order.has_value()) {
    set_clauses.push_back("sort_order = ?");
    db_params.push_back(static_cast<int64_t>(params.sort_order.value()));
  }

  if (set_clauses.empty()) {
    return std::unexpected("No fields to update");
  }

  std::string sql = "UPDATE tags SET " +
                    std::ranges::fold_left(set_clauses, std::string{},
                                           [](const std::string& acc, const std::string& clause) {
                                             return acc.empty() ? clause : acc + ", " + clause;
                                           }) +
                    " WHERE id = ?";

  db_params.push_back(params.id);

  auto result = Core::Database::execute(*app_state.database, sql, db_params);
  if (!result) {
    return std::unexpected("Failed to update tag: " + result.error());
  }

  return {};
}

auto delete_tag(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string> {
  // 数据库中已设置 ON DELETE CASCADE，会自动删除子标签和 asset_tags 关联
  std::string sql = "DELETE FROM tags WHERE id = ?";
  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to delete tag: " + result.error());
  }

  return {};
}

auto list_all_tags(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Tag>, std::string> {
  std::string sql = R"(
            SELECT id, name, parent_id, sort_order, created_at, updated_at
            FROM tags
            ORDER BY sort_order, name
        )";

  auto result = Core::Database::query<Types::Tag>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to list all tags: " + result.error());
  }

  return result.value();
}

// ============= 资产-标签关联操作 =============

auto add_tags_to_asset(Core::State::AppState& app_state, const Types::AddTagsToAssetParams& params)
    -> std::expected<void, std::string> {
  if (params.tag_ids.empty()) {
    return {};  // 没有标签要添加，直接返回成功
  }

  // 使用事务批量插入，使用 INSERT OR IGNORE 避免重复
  return Core::Database::execute_transaction(
      *app_state.database,
      [&](Core::Database::State::DatabaseState& db_state) -> std::expected<void, std::string> {
        std::string sql = R"(
                INSERT OR IGNORE INTO asset_tags (asset_id, tag_id)
                VALUES (?, ?)
            )";

        for (const auto& tag_id : params.tag_ids) {
          std::vector<Core::Database::Types::DbParam> db_params = {params.asset_id, tag_id};
          auto result = Core::Database::execute(db_state, sql, db_params);
          if (!result) {
            return std::unexpected("Failed to add tag to asset: " + result.error());
          }
        }

        return {};
      });
}

auto remove_tags_from_asset(Core::State::AppState& app_state,
                            const Types::RemoveTagsFromAssetParams& params)
    -> std::expected<void, std::string> {
  if (params.tag_ids.empty()) {
    return {};  // 没有标签要移除，直接返回成功
  }

  // 构建 IN 子句
  std::string placeholders = std::string(params.tag_ids.size() * 2 - 1, '?');
  for (size_t i = 1; i < params.tag_ids.size(); ++i) {
    placeholders[i * 2 - 1] = ',';
  }

  std::string sql =
      std::format("DELETE FROM asset_tags WHERE asset_id = ? AND tag_id IN ({})", placeholders);

  std::vector<Core::Database::Types::DbParam> db_params;
  db_params.push_back(params.asset_id);
  for (const auto& tag_id : params.tag_ids) {
    db_params.push_back(tag_id);
  }

  auto result = Core::Database::execute(*app_state.database, sql, db_params);
  if (!result) {
    return std::unexpected("Failed to remove tags from asset: " + result.error());
  }

  return {};
}

auto replace_asset_tags(Core::State::AppState& app_state, std::int64_t asset_id,
                        const std::vector<std::int64_t>& tag_ids)
    -> std::expected<void, std::string> {
  // 使用事务：先删除所有标签，再添加新标签
  return Core::Database::execute_transaction(
      *app_state.database,
      [&](Core::Database::State::DatabaseState& db_state) -> std::expected<void, std::string> {
        // 1. 删除所有现有标签
        std::string delete_sql = "DELETE FROM asset_tags WHERE asset_id = ?";
        std::vector<Core::Database::Types::DbParam> delete_params = {asset_id};
        auto delete_result = Core::Database::execute(db_state, delete_sql, delete_params);
        if (!delete_result) {
          return std::unexpected("Failed to delete existing tags: " + delete_result.error());
        }

        // 2. 添加新标签
        if (!tag_ids.empty()) {
          std::string insert_sql = R"(
                    INSERT INTO asset_tags (asset_id, tag_id)
                    VALUES (?, ?)
                )";

          for (const auto& tag_id : tag_ids) {
            std::vector<Core::Database::Types::DbParam> insert_params = {asset_id, tag_id};
            auto insert_result = Core::Database::execute(db_state, insert_sql, insert_params);
            if (!insert_result) {
              return std::unexpected("Failed to add new tag: " + insert_result.error());
            }
          }
        }

        return {};
      });
}

auto get_asset_tags(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Types::Tag>, std::string> {
  std::string sql = R"(
            SELECT t.id, t.name, t.parent_id, t.sort_order, t.created_at, t.updated_at
            FROM tags t
            INNER JOIN asset_tags at ON t.id = at.tag_id
            WHERE at.asset_id = ?
            ORDER BY t.sort_order, t.name
        )";

  std::vector<Core::Database::Types::DbParam> params = {asset_id};

  auto result = Core::Database::query<Types::Tag>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to get asset tags: " + result.error());
  }

  return result.value();
}

auto get_tags_by_asset_ids(Core::State::AppState& app_state,
                           const std::vector<std::int64_t>& asset_ids)
    -> std::expected<std::unordered_map<std::int64_t, std::vector<Types::Tag>>, std::string> {
  if (asset_ids.empty()) {
    return std::unordered_map<std::int64_t, std::vector<Types::Tag>>{};
  }

  // 构建 IN 子句
  std::string placeholders = std::string(asset_ids.size() * 2 - 1, '?');
  for (size_t i = 1; i < asset_ids.size(); ++i) {
    placeholders[i * 2 - 1] = ',';
  }

  std::string sql = std::format(R"(
            SELECT at.asset_id, t.id, t.name, t.parent_id, t.sort_order, t.created_at, t.updated_at
            FROM tags t
            INNER JOIN asset_tags at ON t.id = at.tag_id
            WHERE at.asset_id IN ({})
            ORDER BY at.asset_id, t.sort_order, t.name
        )",
                                placeholders);

  std::vector<Core::Database::Types::DbParam> params;
  for (const auto& asset_id : asset_ids) {
    params.push_back(asset_id);
  }

  // 定义查询结果结构（包含 asset_id）
  struct AssetTagRow {
    std::int64_t asset_id;
    std::int64_t id;
    std::string name;
    std::optional<std::int64_t> parent_id;
    int sort_order;
    std::int64_t created_at;
    std::int64_t updated_at;
  };

  auto result = Core::Database::query<AssetTagRow>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to get tags by asset ids: " + result.error());
  }

  // 组织结果为映射
  std::unordered_map<std::int64_t, std::vector<Types::Tag>> tag_map;
  for (const auto& row : result.value()) {
    Types::Tag tag{.id = row.id,
                   .name = row.name,
                   .parent_id = row.parent_id,
                   .sort_order = row.sort_order,
                   .created_at = row.created_at,
                   .updated_at = row.updated_at};
    tag_map[row.asset_id].push_back(std::move(tag));
  }

  return tag_map;
}

// ============= 统计功能 =============

auto get_tag_stats(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::TagStats>, std::string> {
  std::string sql = R"(
            SELECT t.id as tag_id, t.name as tag_name, COUNT(at.asset_id) as asset_count
            FROM tags t
            LEFT JOIN asset_tags at ON t.id = at.tag_id
            GROUP BY t.id, t.name
            ORDER BY asset_count DESC, t.name
        )";

  auto result = Core::Database::query<Types::TagStats>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to get tag stats: " + result.error());
  }

  return result.value();
}

// ============= 标签树构建 =============

auto get_tag_tree(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::TagTreeNode>, std::string> {
  // 1. 获取所有标签
  auto tags_result = list_all_tags(app_state);
  if (!tags_result) {
    return std::unexpected("Failed to get all tags: " + tags_result.error());
  }

  const auto& tags = tags_result.value();

  // 2. 查询每个标签的直接资产数量（不包含子标签）
  std::unordered_map<std::int64_t, std::int64_t> direct_asset_counts;
  std::string count_sql = R"(
            SELECT tag_id, COUNT(DISTINCT asset_id) as count
            FROM asset_tags
            GROUP BY tag_id
        )";

  struct TagAssetCount {
    std::int64_t tag_id;
    std::int64_t count;
  };

  auto count_result = Core::Database::query<TagAssetCount>(*app_state.database, count_sql);
  if (!count_result) {
    return std::unexpected("Failed to query asset counts: " + count_result.error());
  }

  // 填充直接资产数量映射
  for (const auto& item : count_result.value()) {
    direct_asset_counts[item.tag_id] = item.count;
  }

  // 3. 创建 id -> TagTreeNode 的映射
  std::unordered_map<std::int64_t, Types::TagTreeNode> node_map;

  // 第一次遍历：创建所有节点
  for (const auto& tag : tags) {
    Types::TagTreeNode node{.id = tag.id,
                            .name = tag.name,
                            .parent_id = tag.parent_id,
                            .sort_order = tag.sort_order,
                            .created_at = tag.created_at,
                            .updated_at = tag.updated_at,
                            .children = {}};

    node_map[tag.id] = std::move(node);
  }

  // 4. 第二次遍历：构建父子关系
  std::unordered_map<std::int64_t, std::vector<std::int64_t>> parent_to_children;
  std::vector<std::int64_t> root_ids;

  for (const auto& tag : tags) {
    if (tag.parent_id.has_value()) {
      parent_to_children[tag.parent_id.value()].push_back(tag.id);
    } else {
      root_ids.push_back(tag.id);
    }
  }

  // 5. 递归构建树结构
  std::function<Types::TagTreeNode(std::int64_t)> build_tree;
  build_tree = [&](std::int64_t tag_id) -> Types::TagTreeNode {
    auto node_it = node_map.find(tag_id);
    if (node_it == node_map.end()) {
      Logger().error("Tag {} not found in node_map", tag_id);
      return Types::TagTreeNode{};
    }

    Types::TagTreeNode node = std::move(node_it->second);

    // 递归构建子节点
    auto children_it = parent_to_children.find(tag_id);
    if (children_it != parent_to_children.end()) {
      for (std::int64_t child_id : children_it->second) {
        node.children.push_back(build_tree(child_id));
      }
    }

    return node;
  };

  // 6. 构建所有根节点
  std::vector<Types::TagTreeNode> root_nodes;
  for (std::int64_t root_id : root_ids) {
    root_nodes.push_back(build_tree(root_id));
  }

  // 7. 对根节点按 sort_order 和 name 排序
  std::sort(root_nodes.begin(), root_nodes.end(),
            [](const Types::TagTreeNode& a, const Types::TagTreeNode& b) {
              if (a.sort_order != b.sort_order) {
                return a.sort_order < b.sort_order;
              }
              return a.name < b.name;
            });

  // 递归排序所有子节点
  std::function<void(Types::TagTreeNode&)> sort_children;
  sort_children = [&](Types::TagTreeNode& node) {
    std::sort(node.children.begin(), node.children.end(),
              [](const Types::TagTreeNode& a, const Types::TagTreeNode& b) {
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

  // 8. 递归计算每个标签的 asset_count（包含所有子标签）
  std::function<std::int64_t(Types::TagTreeNode&)> calculate_total_assets;
  calculate_total_assets = [&](Types::TagTreeNode& node) -> std::int64_t {
    // 当前标签的直接资产数量
    std::int64_t total = 0;
    auto it = direct_asset_counts.find(node.id);
    if (it != direct_asset_counts.end()) {
      total = it->second;
    }

    // 递归累加所有子标签的资产（注意：不重复计算同一资产）
    // 这里简化处理：直接累加，实际可能有资产同时属于父子标签
    for (auto& child : node.children) {
      total += calculate_total_assets(child);
    }

    node.asset_count = total;
    return total;
  };

  // 对所有根节点执行计算
  for (auto& root : root_nodes) {
    calculate_total_assets(root);
  }

  return root_nodes;
}

}  // namespace Features::Gallery::Tag::Repository
