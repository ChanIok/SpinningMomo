module;

module Features.Gallery.Asset.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.Types;
import Utils.Logger;
import Utils.Time;
import <rfl.hpp>;

namespace Features::Gallery::Asset::Repository {

// ============= 基本 CRUD 操作 =============

auto create_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO assets (
                name, path, type,
                width, height, size, mime_type, hash, folder_id,
                file_created_at, file_modified_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(item.name);
  params.push_back(item.filepath);
  params.push_back(item.type);

  // 处理 optional 类型字段
  params.push_back(item.width.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.height.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.size.has_value() ? Core::Database::Types::DbParam{item.size.value()}
                                         : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.mime_type);

  params.push_back(item.hash.has_value() ? Core::Database::Types::DbParam{item.hash.value()}
                                         : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.folder_id.has_value()
                       ? Core::Database::Types::DbParam{item.folder_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.file_created_at.has_value()
                       ? Core::Database::Types::DbParam{item.file_created_at.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.file_modified_at.has_value()
                       ? Core::Database::Types::DbParam{item.file_modified_at.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to insert asset item: " + result.error());
  }

  // 获取插入的 ID
  auto id_result =
      Core::Database::query_scalar<int64_t>(*app_state.database, "SELECT last_insert_rowid()");
  if (!id_result) {
    return std::unexpected("Failed to get inserted ID: " + id_result.error());
  }

  return id_result->value_or(0);
}

auto get_asset_by_id(Core::State::AppState& app_state, int64_t id)
    -> std::expected<std::optional<Types::Asset>, std::string> {
  std::string sql = R"(
            SELECT id, name, path, type,
                   width, height, size, mime_type, hash, folder_id,
                   file_created_at, file_modified_at,
                   created_at, updated_at, deleted_at
            FROM assets
            WHERE id = ? AND deleted_at IS NULL
        )";

  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::query_single<Types::Asset>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query asset item: " + result.error());
  }

  return result.value();
}

auto get_asset_by_filepath(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Asset>, std::string> {
  std::string sql = R"(
            SELECT id, name, path, type,
                   width, height, size, mime_type, hash, folder_id,
                   file_created_at, file_modified_at,
                   created_at, updated_at, deleted_at
            FROM assets
            WHERE path = ? AND deleted_at IS NULL
        )";

  std::vector<Core::Database::Types::DbParam> params = {path};

  auto result = Core::Database::query_single<Types::Asset>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query asset item by path: " + result.error());
  }

  return result.value();
}

auto update_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<void, std::string> {
  std::string sql = R"(
            UPDATE assets SET
                name = ?, path = ?, type = ?,
                width = ?, height = ?, size = ?, mime_type = ?, hash = ?, folder_id = ?,
                file_created_at = ?, file_modified_at = ?
            WHERE id = ?
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(item.name);
  params.push_back(item.filepath);
  params.push_back(item.type);

  // 处理 optional 类型字段
  params.push_back(item.width.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.height.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.size.has_value() ? Core::Database::Types::DbParam{item.size.value()}
                                         : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.mime_type);

  params.push_back(item.hash.has_value() ? Core::Database::Types::DbParam{item.hash.value()}
                                         : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.folder_id.has_value()
                       ? Core::Database::Types::DbParam{item.folder_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.file_created_at.has_value()
                       ? Core::Database::Types::DbParam{item.file_created_at.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.file_modified_at.has_value()
                       ? Core::Database::Types::DbParam{item.file_modified_at.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.id);

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to update asset item: " + result.error());
  }

  return {};
}

auto soft_delete_asset(Core::State::AppState& app_state, int64_t id)
    -> std::expected<void, std::string> {
  std::string sql = "UPDATE assets SET deleted_at = (unixepoch('subsec') * 1000) WHERE id = ?";

  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to soft delete asset item: " + result.error());
  }

  return {};
}

auto hard_delete_asset(Core::State::AppState& app_state, int64_t id)
    -> std::expected<void, std::string> {
  std::string sql = "DELETE FROM assets WHERE id = ?";
  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to hard delete asset item: " + result.error());
  }

  return {};
}

// ============= 查询操作 =============

auto build_asset_list_query_conditions(const Types::ListParams& params) -> AssetQueryBuilder {
  AssetQueryBuilder builder;
  std::vector<std::string> conditions;

  // 基础条件：排除软删除的记录
  conditions.push_back("deleted_at IS NULL");

  // 类型筛选
  if (params.filter_type.has_value() && !params.filter_type->empty()) {
    conditions.push_back("type = ?");
    builder.params.push_back(*params.filter_type);
  }

  // 搜索查询
  if (params.search_query.has_value() && !params.search_query->empty()) {
    conditions.push_back("name LIKE ?");
    builder.params.push_back("%" + *params.search_query + "%");
  }

  // 构建 WHERE 子句
  if (!conditions.empty()) {
    builder.where_clause =
        "WHERE " + std::ranges::fold_left(conditions, std::string{},
                                          [](const std::string& acc, const std::string& cond) {
                                            return acc.empty() ? cond : acc + " AND " + cond;
                                          });
  }

  return builder;
}

auto list_asset(Core::State::AppState& app_state, const Types::ListParams& params)
    -> std::expected<Types::ListResponse, std::string> {
  // 构建查询条件
  auto query_builder = build_asset_list_query_conditions(params);

  // 获取总数
  std::string count_sql = "SELECT COUNT(*) FROM assets " + query_builder.where_clause;
  auto count_result =
      Core::Database::query_scalar<int>(*app_state.database, count_sql, query_builder.params);
  if (!count_result) {
    return std::unexpected("Failed to count asset items: " + count_result.error());
  }
  int total_count = count_result->value_or(0);

  // 构建分页和排序
  int page = params.page.value_or(1);
  int per_page = params.per_page.value_or(50);
  std::string sort_by = params.sort_by.value_or("created_at");
  std::string sort_order = params.sort_order.value_or("desc");

  // 安全的排序字段验证
  if (sort_by != "created_at" && sort_by != "name" && sort_by != "size") {
    sort_by = "created_at";
  }
  if (sort_order != "asc" && sort_order != "desc") {
    sort_order = "desc";
  }

  int offset = (page - 1) * per_page;

  // 构建主查询
  std::string sql = std::format(R"(
            SELECT id, name, path as filepath, type,
                   width, height, size, mime_type, hash, folder_id,
                   file_created_at, file_modified_at,
                   created_at, updated_at, deleted_at
            FROM assets
            {}
            ORDER BY {} {}
            LIMIT ? OFFSET ?
        )",
                                query_builder.where_clause, sort_by, sort_order);

  // 添加分页参数
  auto final_params = query_builder.params;
  final_params.push_back(per_page);
  final_params.push_back(offset);

  auto items_result = Core::Database::query<Types::Asset>(*app_state.database, sql, final_params);
  if (!items_result) {
    return std::unexpected("Failed to query asset items: " + items_result.error());
  }

  // 构建响应
  Types::ListResponse response;
  response.items = std::move(items_result.value());
  response.total_count = total_count;
  response.current_page = page;
  response.per_page = per_page;
  response.total_pages = (total_count + per_page - 1) / per_page;

  return response;
}

auto count_asset(Core::State::AppState& app_state, const std::optional<std::string>& filter_type,
                 const std::optional<std::string>& search_query)
    -> std::expected<int, std::string> {
  Types::ListParams params;
  params.filter_type = filter_type;
  params.search_query = search_query;

  auto query_builder = build_asset_list_query_conditions(params);

  std::string sql = "SELECT COUNT(*) FROM assets " + query_builder.where_clause;
  auto result = Core::Database::query_scalar<int>(*app_state.database, sql, query_builder.params);
  if (!result) {
    return std::unexpected("Failed to count asset items: " + result.error());
  }

  return result->value_or(0);
}

auto get_asset_stats(Core::State::AppState& app_state, const Types::GetStatsParams& params)
    -> std::expected<Types::Stats, std::string> {
  std::string base_where = "WHERE deleted_at IS NULL";
  std::vector<Core::Database::Types::DbParam> where_params;

  Types::Stats stats;

  // 总数
  std::string total_sql = "SELECT COUNT(*) FROM assets " + base_where;
  auto total_result =
      Core::Database::query_scalar<int>(*app_state.database, total_sql, where_params);
  if (!total_result) {
    return std::unexpected("Failed to get total count: " + total_result.error());
  }
  stats.total_count = total_result->value_or(0);

  // 按类型统计
  std::string type_sql = "SELECT type, COUNT(*) FROM assets " + base_where + " GROUP BY type";
  auto type_result =
      Core::Database::query<Types::TypeCountResult>(*app_state.database, type_sql, where_params);
  if (type_result) {
    for (const auto& result : type_result.value()) {
      if (result.type == "photo")
        stats.photo_count = result.count;
      else if (result.type == "video")
        stats.video_count = result.count;
      else if (result.type == "live_photo")
        stats.live_photo_count = result.count;
    }
  }

  // 总大小
  std::string size_sql = "SELECT SUM(size) FROM assets " + base_where;
  auto size_result =
      Core::Database::query_scalar<int64_t>(*app_state.database, size_sql, where_params);
  if (size_result) {
    stats.total_size = size_result->value_or(0);
  }

  // 最早和最新时间
  std::string oldest_sql = "SELECT MIN(created_at) FROM assets " + base_where;
  auto oldest_result =
      Core::Database::query_scalar<std::string>(*app_state.database, oldest_sql, where_params);
  if (oldest_result && oldest_result->has_value()) {
    stats.oldest_item_date = oldest_result.value().value();
  }

  std::string newest_sql = "SELECT MAX(created_at) FROM assets " + base_where;
  auto newest_result =
      Core::Database::query_scalar<std::string>(*app_state.database, newest_sql, where_params);
  if (newest_result && newest_result->has_value()) {
    stats.newest_item_date = newest_result.value().value();
  }

  return stats;
}

auto cleanup_soft_deleted_assets(Core::State::AppState& app_state, int days_old)
    -> std::expected<int, std::string> {
  std::string cutoff_date = std::format(
      "{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now() - std::chrono::days(days_old));

  std::string sql = "DELETE FROM assets WHERE deleted_at IS NOT NULL AND deleted_at < ?";
  std::vector<Core::Database::Types::DbParam> params = {cutoff_date};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to cleanup soft deleted items: " + result.error());
  }

  // 获取删除的行数（这是简化版，实际可能需要在执行前先计数）
  std::string count_sql = "SELECT changes()";
  auto count_result = Core::Database::query_scalar<int>(*app_state.database, count_sql);
  if (!count_result) {
    Logger().warn("Could not get cleanup count, but cleanup operation succeeded");
    return 0;
  }

  return count_result->value_or(0);
}

// 加载数据库中的资产到内存缓存
auto load_asset_cache(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Types::Metadata>, std::string> {
  std::string sql = R"(
    SELECT id, name, path as filepath, type,
           width, height, size, mime_type, hash, folder_id,
           file_created_at, file_modified_at,
           created_at, updated_at, deleted_at
    FROM assets
    WHERE deleted_at IS NULL
  )";

  auto result = Core::Database::query<Types::Asset>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to load asset cache: " + result.error());
  }

  auto assets = std::move(result.value());
  std::unordered_map<std::string, Types::Metadata> cache;
  cache.reserve(assets.size());
  for (const auto& asset : assets) {
    Types::Metadata metadata{.id = asset.id,
                             .filepath = asset.filepath,
                             .size = asset.size.value_or(0),
                             .file_modified_at = asset.file_modified_at.value_or(0),
                             .hash = asset.hash.value_or("")};

    cache.emplace(asset.filepath, std::move(metadata));
  }
  Logger().info("Loaded {} assets into memory cache", cache.size());
  return cache;
}

auto batch_create_asset(Core::State::AppState& app_state, const std::vector<Types::Asset>& items)
    -> std::expected<std::vector<std::int64_t>, std::string> {
  if (items.empty()) {
    return std::vector<int64_t>{};
  }

  std::string insert_prefix = R"(
    INSERT INTO assets (
      name, path, type,
      width, height, size, mime_type, hash, folder_id,
      file_created_at, file_modified_at
    ) VALUES 
  )";

  std::string values_placeholder = "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  // 参数提取器，将Asset对象转换为参数列表
  auto param_extractor =
      [](const Types::Asset& item) -> std::vector<Core::Database::Types::DbParam> {
    std::vector<Core::Database::Types::DbParam> params;
    params.reserve(11);  // 11个字段

    params.push_back(item.name);
    params.push_back(item.filepath);
    params.push_back(item.type);

    params.push_back(item.width.has_value()
                         ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                         : Core::Database::Types::DbParam{std::monostate{}});
    params.push_back(item.height.has_value()
                         ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                         : Core::Database::Types::DbParam{std::monostate{}});
    params.push_back(item.size.has_value() ? Core::Database::Types::DbParam{item.size.value()}
                                           : Core::Database::Types::DbParam{std::monostate{}});

    params.push_back(item.mime_type);

    params.push_back(item.hash.has_value() ? Core::Database::Types::DbParam{item.hash.value()}
                                           : Core::Database::Types::DbParam{std::monostate{}});

    params.push_back(item.folder_id.has_value()
                         ? Core::Database::Types::DbParam{item.folder_id.value()}
                         : Core::Database::Types::DbParam{std::monostate{}});

    params.push_back(item.file_created_at.has_value()
                         ? Core::Database::Types::DbParam{item.file_created_at.value()}
                         : Core::Database::Types::DbParam{std::monostate{}});
    params.push_back(item.file_modified_at.has_value()
                         ? Core::Database::Types::DbParam{item.file_modified_at.value()}
                         : Core::Database::Types::DbParam{std::monostate{}});

    return params;
  };

  // 使用批量插入接口，自动处理分批
  return Core::Database::execute_batch_insert(*app_state.database, insert_prefix,
                                              values_placeholder, items, param_extractor);
}

auto batch_update_asset(Core::State::AppState& app_state, const std::vector<Types::Asset>& items)
    -> std::expected<void, std::string> {
  if (items.empty()) {
    return {};
  }

  // 使用事务批量执行update（SQLite不支持bulk update）
  return Core::Database::execute_transaction(
      *app_state.database,
      [&](Core::Database::State::DatabaseState& db_state) -> std::expected<void, std::string> {
        std::string sql = R"(
          UPDATE assets SET
            name = ?, path = ?, type = ?,
            width = ?, height = ?, size = ?, mime_type = ?, hash = ?, folder_id = ?,
            file_created_at = ?, file_modified_at = ?
          WHERE id = ?
        )";

        // 提取参数的lambda，避免在循环中重复代码
        auto extract_params = [](const Types::Asset& item) {
          std::vector<Core::Database::Types::DbParam> params;
          params.reserve(12);  // 11个更新字段 + 1个WHERE条件

          params.push_back(item.name);
          params.push_back(item.filepath);
          params.push_back(item.type);

          params.push_back(
              item.width.has_value()
                  ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                  : Core::Database::Types::DbParam{std::monostate{}});
          params.push_back(
              item.height.has_value()
                  ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                  : Core::Database::Types::DbParam{std::monostate{}});
          params.push_back(item.size.has_value()
                               ? Core::Database::Types::DbParam{item.size.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(item.mime_type);

          params.push_back(item.hash.has_value()
                               ? Core::Database::Types::DbParam{item.hash.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(item.folder_id.has_value()
                               ? Core::Database::Types::DbParam{item.folder_id.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(item.file_created_at.has_value()
                               ? Core::Database::Types::DbParam{item.file_created_at.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});
          params.push_back(item.file_modified_at.has_value()
                               ? Core::Database::Types::DbParam{item.file_modified_at.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(item.id);  // WHERE id = ?

          return params;
        };

        // 执行批量更新
        for (const auto& item : items) {
          auto params = extract_params(item);
          auto result = Core::Database::execute(db_state, sql, params);
          if (!result) {
            return std::unexpected("Failed to update asset item (id=" + std::to_string(item.id) +
                                   "): " + result.error());
          }
        }

        return {};
      });
}
}  // namespace Features::Gallery::Asset::Repository
