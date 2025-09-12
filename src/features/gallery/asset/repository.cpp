module;

#include <rfl.hpp>  // 用于反射

module Features.Gallery.Asset.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.Types;
import Features.Gallery.Types;
import Utils.Logger;

namespace Features::Gallery::Asset::Repository {

// ============= 基本 CRUD 操作 =============

auto create_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO assets (
                filename, filepath, relative_path, type,
                width, height, file_size, mime_type, file_hash,
                created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(item.filename);
  params.push_back(item.filepath);
  params.push_back(item.relative_path);
  params.push_back(item.type);

  // 处理 optional 类型字段
  params.push_back(item.width.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.height.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.file_size.has_value()
                       ? Core::Database::Types::DbParam{item.file_size.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.mime_type);

  params.push_back(item.file_hash.has_value()
                       ? Core::Database::Types::DbParam{item.file_hash.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.created_at);
  params.push_back(item.updated_at);

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
            SELECT id, filename, filepath, relative_path, type,
                   width, height, file_size, mime_type, file_hash,
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

auto get_asset_by_filepath(Core::State::AppState& app_state, const std::string& filepath)
    -> std::expected<std::optional<Types::Asset>, std::string> {
  std::string sql = R"(
            SELECT id, filename, filepath, relative_path, type,
                   width, height, file_size, mime_type, file_hash,
                   created_at, updated_at, deleted_at
            FROM assets
            WHERE filepath = ? AND deleted_at IS NULL
        )";

  std::vector<Core::Database::Types::DbParam> params = {filepath};

  auto result = Core::Database::query_single<Types::Asset>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query asset item by filepath: " + result.error());
  }

  return result.value();
}

auto update_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<void, std::string> {
  std::string sql = R"(
            UPDATE assets SET
                filename = ?, filepath = ?, relative_path = ?, type = ?,
                width = ?, height = ?, file_size = ?, mime_type = ?, file_hash = ?,
                updated_at = ?
            WHERE id = ?
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(item.filename);
  params.push_back(item.filepath);
  params.push_back(item.relative_path);
  params.push_back(item.type);

  // 处理 optional 类型字段
  params.push_back(item.width.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.height.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.file_size.has_value()
                       ? Core::Database::Types::DbParam{item.file_size.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.mime_type);

  params.push_back(item.file_hash.has_value()
                       ? Core::Database::Types::DbParam{item.file_hash.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.updated_at);

  params.push_back(item.id);

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to update asset item: " + result.error());
  }

  return {};
}

auto soft_delete_asset(Core::State::AppState& app_state, int64_t id)
    -> std::expected<void, std::string> {
  std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());
  std::string sql = "UPDATE assets SET deleted_at = ? WHERE id = ?";

  std::vector<Core::Database::Types::DbParam> params = {timestamp, id};

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
    conditions.push_back("filename LIKE ?");
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
  if (sort_by != "created_at" && sort_by != "filename" && sort_by != "file_size") {
    sort_by = "created_at";
  }
  if (sort_order != "asc" && sort_order != "desc") {
    sort_order = "desc";
  }

  int offset = (page - 1) * per_page;

  // 构建主查询
  std::string sql = std::format(R"(
            SELECT id, filename, filepath, relative_path, type,
                   width, height, file_size, mime_type, file_hash,
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
  response.items = std::move(*items_result);
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
  std::string size_sql = "SELECT SUM(file_size) FROM assets " + base_where;
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
    stats.oldest_item_date = *oldest_result.value();
  }

  std::string newest_sql = "SELECT MAX(created_at) FROM assets " + base_where;
  auto newest_result =
      Core::Database::query_scalar<std::string>(*app_state.database, newest_sql, where_params);
  if (newest_result && newest_result->has_value()) {
    stats.newest_item_date = *newest_result.value();
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
    -> std::expected<Types::Cache, std::string> {
  std::string sql = R"(
    SELECT id, filename, filepath, relative_path, type,
           width, height, file_size, mime_type, file_hash,
           created_at, updated_at, deleted_at
    FROM assets
    WHERE deleted_at IS NULL
  )";

  auto result = Core::Database::query<Types::Asset>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to load asset cache: " + result.error());
  }

  auto assets = std::move(*result);
  Types::Cache cache;
  cache.reserve(assets.size());

  for (const auto& asset : assets) {
    Types::Metadata metadata;
    metadata.id = asset.id;
    metadata.filepath = asset.filepath;
    metadata.file_size = asset.file_size.value_or(0);
    metadata.last_modified = asset.updated_at;

    if (asset.file_hash && !asset.file_hash->empty()) {
      metadata.file_hash = *asset.file_hash;
    }

    cache[asset.filepath] = std::move(metadata);
  }

  Logger().info("Loaded {} assets into memory cache", cache.size());
  return cache;
}

auto batch_create_asset(Core::State::AppState& app_state, const std::vector<Types::Asset>& items)
    -> std::expected<std::vector<std::int64_t>, std::string> {
  if (items.empty()) {
    return std::vector<int64_t>{};
  }

  // 构建批量插入SQL
  std::string sql = R"(
    INSERT INTO assets (
      filename, filepath, relative_path, type,
      width, height, file_size, mime_type, file_hash,
      created_at, updated_at
    ) VALUES
  )";

  std::vector<std::string> value_placeholders;
  std::vector<Core::Database::Types::DbParam> all_params;
  value_placeholders.reserve(items.size());
  all_params.reserve(items.size() * 11);  // 11个字段

  for (const auto& item : items) {
    value_placeholders.push_back("(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    // 按顺序添加所有参数
    all_params.push_back(item.filename);
    all_params.push_back(item.filepath);
    all_params.push_back(item.relative_path);
    all_params.push_back(item.type);

    all_params.push_back(
        item.width.has_value()
            ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
            : Core::Database::Types::DbParam{std::monostate{}});
    all_params.push_back(
        item.height.has_value()
            ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
            : Core::Database::Types::DbParam{std::monostate{}});
    all_params.push_back(item.file_size.has_value()
                             ? Core::Database::Types::DbParam{item.file_size.value()}
                             : Core::Database::Types::DbParam{std::monostate{}});

    all_params.push_back(item.mime_type);

    all_params.push_back(item.file_hash.has_value()
                             ? Core::Database::Types::DbParam{item.file_hash.value()}
                             : Core::Database::Types::DbParam{std::monostate{}});

    all_params.push_back(item.created_at);
    all_params.push_back(item.updated_at);
  }

  // 合并所有 VALUES 子句
  for (size_t i = 0; i < value_placeholders.size(); ++i) {
    if (i > 0) sql += ", ";
    sql += value_placeholders[i];
  }

  // 在事务中执行批量插入
  return Core::Database::execute_transaction(
      *app_state.database,
      [&](Core::Database::State::DatabaseState& db_state)
          -> std::expected<std::vector<int64_t>, std::string> {
        auto result = Core::Database::execute(db_state, sql, all_params);
        if (!result) {
          return std::unexpected("Failed to execute batch insert: " + result.error());
        }

        // 获取批量插入的ID范围
        auto last_id_result =
            Core::Database::query_scalar<int64_t>(db_state, "SELECT last_insert_rowid()");
        if (!last_id_result || !last_id_result->has_value()) {
          return std::unexpected("Failed to get last insert ID");
        }

        int64_t last_id = last_id_result->value();
        int64_t first_id = last_id - static_cast<int64_t>(items.size()) + 1;

        // 生成ID列表
        std::vector<int64_t> inserted_ids;
        inserted_ids.reserve(items.size());
        for (int64_t id = first_id; id <= last_id; ++id) {
          inserted_ids.push_back(id);
        }

        return inserted_ids;
      });
}

auto batch_update_asset(Core::State::AppState& app_state, const std::vector<Types::Asset>& items)
    -> std::expected<void, std::string> {
  if (items.empty()) {
    return {};
  }

  // 使用事务批量执行update（目前SQLite不支持真正的bulk update）
  return Core::Database::execute_transaction(
      *app_state.database,
      [&](Core::Database::State::DatabaseState& db_state) -> std::expected<void, std::string> {
        std::string sql = R"(
        UPDATE assets SET
          filename = ?, filepath = ?, relative_path = ?, type = ?,
          width = ?, height = ?, file_size = ?, mime_type = ?, file_hash = ?,
          updated_at = ?
        WHERE id = ?
      )";

        for (const auto& item : items) {
          std::vector<Core::Database::Types::DbParam> params;
          params.push_back(item.filename);
          params.push_back(item.filepath);
          params.push_back(item.relative_path);
          params.push_back(item.type);

          params.push_back(
              item.width.has_value()
                  ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                  : Core::Database::Types::DbParam{std::monostate{}});
          params.push_back(
              item.height.has_value()
                  ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                  : Core::Database::Types::DbParam{std::monostate{}});
          params.push_back(item.file_size.has_value()
                               ? Core::Database::Types::DbParam{item.file_size.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(item.mime_type);

          params.push_back(item.file_hash.has_value()
                               ? Core::Database::Types::DbParam{item.file_hash.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(item.updated_at);

          params.push_back(item.id);

          auto result = Core::Database::execute(db_state, sql, params);
          if (!result) {
            return std::unexpected("Failed to update asset item: " + result.error());
          }
        }

        return {};
      });
}
}  // namespace Features::Gallery::Asset::Repository
