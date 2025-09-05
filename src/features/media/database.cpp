module;

#include <rfl.hpp>  // 用于反射

module Features.Media.Database;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Core.Database.DataMapper;
import Features.Media.Types;
import Utils.Logger;

namespace Features::Media::Database {

// ============= 基本 CRUD 操作 =============

auto create_media_item(Core::State::AppState& app_state, const Types::MediaItem& item)
    -> std::expected<int64_t, std::string> {
  try {
    std::string sql = R"(
            INSERT INTO media_items (
                filename, filepath, relative_path, type,
                width, height, file_size, mime_type,
                created_at, updated_at, thumbnail_path
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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
    params.push_back(item.created_at);
    params.push_back(item.updated_at);

    params.push_back(item.thumbnail_path.has_value()
                         ? Core::Database::Types::DbParam{item.thumbnail_path.value()}
                         : Core::Database::Types::DbParam{std::monostate{}});

    auto result = Core::Database::execute(*app_state.database, sql, params);
    if (!result) {
      return std::unexpected("Failed to insert media item: " + result.error());
    }

    // 获取插入的 ID
    auto id_result =
        Core::Database::query_scalar<int64_t>(*app_state.database, "SELECT last_insert_rowid()");
    if (!id_result) {
      return std::unexpected("Failed to get inserted ID: " + id_result.error());
    }

    return id_result->value_or(0);

  } catch (const std::exception& e) {
    return std::unexpected("Exception in create_media_item: " + std::string(e.what()));
  }
}

auto get_media_item_by_id(Core::State::AppState& app_state, int64_t id)
    -> std::expected<std::optional<Types::MediaItem>, std::string> {
  try {
    std::string sql = R"(
            SELECT id, filename, filepath, relative_path, type,
                   width, height, file_size, mime_type,
                   created_at, updated_at, deleted_at, thumbnail_path
            FROM media_items 
            WHERE id = ? AND deleted_at IS NULL
        )";

    std::vector<Core::Database::Types::DbParam> params = {id};

    auto result = Core::Database::query_single<Types::MediaItem>(*app_state.database, sql, params);
    if (!result) {
      return std::unexpected("Failed to query media item: " + result.error());
    }

    return result.value();

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_media_item_by_id: " + std::string(e.what()));
  }
}

auto get_media_item_by_filepath(Core::State::AppState& app_state, const std::string& filepath)
    -> std::expected<std::optional<Types::MediaItem>, std::string> {
  try {
    std::string sql = R"(
            SELECT id, filename, filepath, relative_path, type,
                   width, height, file_size, mime_type,
                   created_at, updated_at, deleted_at, thumbnail_path
            FROM media_items 
            WHERE filepath = ? AND deleted_at IS NULL
        )";

    std::vector<Core::Database::Types::DbParam> params = {filepath};

    auto result = Core::Database::query_single<Types::MediaItem>(*app_state.database, sql, params);
    if (!result) {
      return std::unexpected("Failed to query media item by filepath: " + result.error());
    }

    return result.value();

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_media_item_by_filepath: " + std::string(e.what()));
  }
}

auto update_media_item(Core::State::AppState& app_state, const Types::MediaItem& item)
    -> std::expected<void, std::string> {
  try {
    std::string sql = R"(
            UPDATE media_items SET
                filename = ?, filepath = ?, relative_path = ?, type = ?,
                width = ?, height = ?, file_size = ?, mime_type = ?,
                updated_at = ?, thumbnail_path = ?
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
    params.push_back(item.updated_at);

    params.push_back(item.thumbnail_path.has_value()
                         ? Core::Database::Types::DbParam{item.thumbnail_path.value()}
                         : Core::Database::Types::DbParam{std::monostate{}});

    params.push_back(item.id);

    auto result = Core::Database::execute(*app_state.database, sql, params);
    if (!result) {
      return std::unexpected("Failed to update media item: " + result.error());
    }

    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception in update_media_item: " + std::string(e.what()));
  }
}

auto soft_delete_media_item(Core::State::AppState& app_state, int64_t id)
    -> std::expected<void, std::string> {
  try {
    std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());
    std::string sql = "UPDATE media_items SET deleted_at = ? WHERE id = ?";

    std::vector<Core::Database::Types::DbParam> params = {timestamp, id};

    auto result = Core::Database::execute(*app_state.database, sql, params);
    if (!result) {
      return std::unexpected("Failed to soft delete media item: " + result.error());
    }

    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception in soft_delete_media_item: " + std::string(e.what()));
  }
}

auto hard_delete_media_item(Core::State::AppState& app_state, int64_t id)
    -> std::expected<void, std::string> {
  try {
    std::string sql = "DELETE FROM media_items WHERE id = ?";
    std::vector<Core::Database::Types::DbParam> params = {id};

    auto result = Core::Database::execute(*app_state.database, sql, params);
    if (!result) {
      return std::unexpected("Failed to hard delete media item: " + result.error());
    }

    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception in hard_delete_media_item: " + std::string(e.what()));
  }
}

// ============= 查询操作 =============

auto build_list_query_conditions(const Types::ListMediaParams& params) -> QueryBuilder {
  QueryBuilder builder;
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

auto list_media_items(Core::State::AppState& app_state, const Types::ListMediaParams& params)
    -> std::expected<Types::MediaListResponse, std::string> {
  try {
    // 构建查询条件
    auto query_builder = build_list_query_conditions(params);

    // 获取总数
    std::string count_sql = "SELECT COUNT(*) FROM media_items " + query_builder.where_clause;
    auto count_result =
        Core::Database::query_scalar<int>(*app_state.database, count_sql, query_builder.params);
    if (!count_result) {
      return std::unexpected("Failed to count media items: " + count_result.error());
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
                   width, height, file_size, mime_type,
                   created_at, updated_at, deleted_at, thumbnail_path
            FROM media_items 
            {} 
            ORDER BY {} {} 
            LIMIT ? OFFSET ?
        )",
                                  query_builder.where_clause, sort_by, sort_order);

    // 添加分页参数
    auto final_params = query_builder.params;
    final_params.push_back(per_page);
    final_params.push_back(offset);

    auto items_result =
        Core::Database::query<Types::MediaItem>(*app_state.database, sql, final_params);
    if (!items_result) {
      return std::unexpected("Failed to query media items: " + items_result.error());
    }

    // 构建响应
    Types::MediaListResponse response;
    response.items = std::move(*items_result);
    response.total_count = total_count;
    response.current_page = page;
    response.per_page = per_page;
    response.total_pages = (total_count + per_page - 1) / per_page;

    return response;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in list_media_items: " + std::string(e.what()));
  }
}

auto count_media_items(Core::State::AppState& app_state,
                       const std::optional<std::string>& filter_type,
                       const std::optional<std::string>& search_query)
    -> std::expected<int, std::string> {
  try {
    Types::ListMediaParams params;
    params.filter_type = filter_type;
    params.search_query = search_query;

    auto query_builder = build_list_query_conditions(params);

    std::string sql = "SELECT COUNT(*) FROM media_items " + query_builder.where_clause;
    auto result = Core::Database::query_scalar<int>(*app_state.database, sql, query_builder.params);
    if (!result) {
      return std::unexpected("Failed to count media items: " + result.error());
    }

    return result->value_or(0);

  } catch (const std::exception& e) {
    return std::unexpected("Exception in count_media_items: " + std::string(e.what()));
  }
}

auto get_media_stats(Core::State::AppState& app_state, const Types::GetStatsParams& params)
    -> std::expected<Types::MediaStats, std::string> {
  try {
    std::string base_where = "WHERE deleted_at IS NULL";
    std::vector<Core::Database::Types::DbParam> where_params;

    Types::MediaStats stats;

    // 总数
    std::string total_sql = "SELECT COUNT(*) FROM media_items " + base_where;
    auto total_result =
        Core::Database::query_scalar<int>(*app_state.database, total_sql, where_params);
    if (!total_result) {
      return std::unexpected("Failed to get total count: " + total_result.error());
    }
    stats.total_count = total_result->value_or(0);

    // 按类型统计
    std::string type_sql =
        "SELECT type, COUNT(*) FROM media_items " + base_where + " GROUP BY type";
    auto type_result =
        Core::Database::query<Types::TypeCountResult>(*app_state.database, type_sql, where_params);
    if (type_result) {
      for (const auto& result : *type_result) {
        if (result.type == "photo")
          stats.photo_count = result.count;
        else if (result.type == "video")
          stats.video_count = result.count;
        else if (result.type == "live_photo")
          stats.live_photo_count = result.count;
      }
    }

    // 总大小
    std::string size_sql = "SELECT SUM(file_size) FROM media_items " + base_where;
    auto size_result =
        Core::Database::query_scalar<int64_t>(*app_state.database, size_sql, where_params);
    if (size_result) {
      stats.total_size = size_result->value_or(0);
    }

    // 最早和最新时间
    std::string oldest_sql = "SELECT MIN(created_at) FROM media_items " + base_where;
    auto oldest_result =
        Core::Database::query_scalar<std::string>(*app_state.database, oldest_sql, where_params);
    if (oldest_result && oldest_result->has_value()) {
      stats.oldest_item_date = *oldest_result.value();
    }

    std::string newest_sql = "SELECT MAX(created_at) FROM media_items " + base_where;
    auto newest_result =
        Core::Database::query_scalar<std::string>(*app_state.database, newest_sql, where_params);
    if (newest_result && newest_result->has_value()) {
      stats.newest_item_date = *newest_result.value();
    }

    return stats;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in get_media_stats: " + std::string(e.what()));
  }
}

// ============= 辅助函数 =============

auto filepath_exists(Core::State::AppState& app_state, const std::string& filepath)
    -> std::expected<bool, std::string> {
  try {
    std::string sql = "SELECT COUNT(*) FROM media_items WHERE filepath = ? AND deleted_at IS NULL";
    std::vector<Core::Database::Types::DbParam> params = {filepath};

    auto result = Core::Database::query_scalar<int>(*app_state.database, sql, params);
    if (!result) {
      return std::unexpected("Failed to check filepath existence: " + result.error());
    }

    return result->value_or(0) > 0;

  } catch (const std::exception& e) {
    return std::unexpected("Exception in filepath_exists: " + std::string(e.what()));
  }
}

auto cleanup_soft_deleted(Core::State::AppState& app_state, int days_old)
    -> std::expected<int, std::string> {
  try {
    std::string cutoff_date = std::format(
        "{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now() - std::chrono::days(days_old));

    std::string sql = "DELETE FROM media_items WHERE deleted_at IS NOT NULL AND deleted_at < ?";
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

  } catch (const std::exception& e) {
    return std::unexpected("Exception in cleanup_soft_deleted: " + std::string(e.what()));
  }
}

auto batch_create_media_items(Core::State::AppState& app_state,
                              const std::vector<Types::MediaItem>& items)
    -> std::expected<std::vector<int64_t>, std::string> {
  std::vector<int64_t> inserted_ids;
  inserted_ids.reserve(items.size());

  for (const auto& item : items) {
    auto result = create_media_item(app_state, item);
    if (result) {
      inserted_ids.push_back(*result);
    } else {
      // 如果中途失败，返回已成功插入的 ID 列表和错误信息
      return std::unexpected("Batch create failed at one of the items: " + result.error());
    }
  }
  return inserted_ids;
}

auto batch_update_media_items(Core::State::AppState& app_state,
                              const std::vector<Types::MediaItem>& items)
    -> std::expected<void, std::string> {
  for (const auto& item : items) {
    auto result = update_media_item(app_state, item);
    if (!result) {
      return std::unexpected("Batch update failed at one of the items: " + result.error());
    }
  }
  return {};
}
}  // namespace Features::Media::Database
