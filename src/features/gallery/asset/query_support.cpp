module;

module Features.Gallery.Asset.QuerySupport;

import std;
import Core.Database;
import Core.Database.Types;
import Core.State;
import Features.Gallery.Color.Filter;
import Features.Gallery.Types;

namespace Features::Gallery::Asset::QuerySupport {

auto validate_month_format(const std::string& month) -> bool {
  if (month.length() != 7 || month[4] != '-') {
    return false;
  }

  for (size_t i = 0; i < month.length(); ++i) {
    if (i == 4) continue;
    if (!std::isdigit(static_cast<unsigned char>(month[i]))) {
      return false;
    }
  }

  int month_num = std::stoi(month.substr(5, 2));
  return month_num >= 1 && month_num <= 12;
}

auto build_in_clause_placeholders(std::size_t count) -> std::string {
  if (count == 0) {
    return "";
  }

  std::string placeholders;
  placeholders.reserve(count * 2 - 1);
  for (std::size_t i = 0; i < count; ++i) {
    if (i > 0) {
      placeholders += ",";
    }
    placeholders += "?";
  }

  return placeholders;
}

auto qualify_asset_column(std::string_view column, std::string_view asset_table_alias)
    -> std::string {
  if (asset_table_alias.empty()) {
    return std::string(column);
  }

  return std::string(asset_table_alias) + "." + std::string(column);
}

auto is_valid_review_flag(const std::string& review_flag) -> bool {
  return review_flag == "none" || review_flag == "picked" || review_flag == "rejected";
}

auto build_query_order_config(std::optional<std::string> sort_by_param,
                              std::optional<std::string> sort_order_param) -> QueryOrderConfig {
  QueryOrderConfig config{
      .sort_by = sort_by_param.value_or("created_at"),
      .sort_order = sort_order_param.value_or("desc"),
  };

  if (config.sort_by != "created_at" && config.sort_by != "name" &&
      config.sort_by != "resolution" && config.sort_by != "size" &&
      config.sort_by != "file_created_at") {
    config.sort_by = "created_at";
  }
  if (config.sort_order != "asc" && config.sort_order != "desc") {
    config.sort_order = "desc";
  }

  if (config.sort_by == "created_at") {
    config.asset_order_clause =
        std::format("ORDER BY COALESCE(file_created_at, created_at) {}, id {}", config.sort_order,
                    config.sort_order);
    config.indexed_order_clause =
        std::format("ORDER BY sort_created_at {}, id {}", config.sort_order, config.sort_order);
    return config;
  }

  if (config.sort_by == "file_created_at") {
    config.asset_order_clause =
        std::format("ORDER BY file_created_at {}, id {}", config.sort_order, config.sort_order);
    config.indexed_order_clause = std::format("ORDER BY sort_file_created_at {}, id {}",
                                              config.sort_order, config.sort_order);
    return config;
  }

  if (config.sort_by == "name") {
    config.asset_order_clause =
        std::format("ORDER BY name {}, id {}", config.sort_order, config.sort_order);
    config.indexed_order_clause =
        std::format("ORDER BY sort_name {}, id {}", config.sort_order, config.sort_order);
    return config;
  }

  if (config.sort_by == "resolution") {
    config.asset_order_clause = std::format(
        "ORDER BY (COALESCE(width, 0) * COALESCE(height, 0)) {}, width {}, height {}, id {}",
        config.sort_order, config.sort_order, config.sort_order, config.sort_order);
    config.indexed_order_clause =
        std::format("ORDER BY sort_resolution {}, sort_width {}, sort_height {}, id {}",
                    config.sort_order, config.sort_order, config.sort_order, config.sort_order);
    return config;
  }

  config.asset_order_clause =
      std::format("ORDER BY size {}, id {}", config.sort_order, config.sort_order);
  config.indexed_order_clause =
      std::format("ORDER BY sort_size {}, id {}", config.sort_order, config.sort_order);
  return config;
}

auto build_unified_where_clause(const Features::Gallery::Types::QueryAssetsFilters& filters,
                                std::string_view asset_table_alias)
    -> std::expected<std::pair<std::string, std::vector<Core::Database::Types::DbParam>>,
                     std::string> {
  std::vector<std::string> conditions;
  std::vector<Core::Database::Types::DbParam> params;
  const auto folder_id_column = qualify_asset_column("folder_id", asset_table_alias);
  const auto file_created_at_column = qualify_asset_column("file_created_at", asset_table_alias);
  const auto created_at_column = qualify_asset_column("created_at", asset_table_alias);
  const auto type_column = qualify_asset_column("type", asset_table_alias);
  const auto name_column = qualify_asset_column("name", asset_table_alias);
  const auto id_column = qualify_asset_column("id", asset_table_alias);
  const auto rating_column = qualify_asset_column("rating", asset_table_alias);
  const auto review_flag_column = qualify_asset_column("review_flag", asset_table_alias);

  if (filters.folder_id.has_value()) {
    if (filters.include_subfolders.value_or(false)) {
      conditions.push_back(std::format(R"({} IN (
        WITH RECURSIVE folder_hierarchy AS (
          SELECT id FROM folders WHERE id = ?
          UNION ALL
          SELECT f.id FROM folders f
          INNER JOIN folder_hierarchy fh ON f.parent_id = fh.id
        )
        SELECT id FROM folder_hierarchy
      ))",
                                       folder_id_column));
      params.push_back(filters.folder_id.value());
    } else {
      conditions.push_back(folder_id_column + " = ?");
      params.push_back(filters.folder_id.value());
    }
  }

  if (filters.month.has_value()) {
    if (validate_month_format(filters.month.value())) {
      conditions.push_back(
          std::format("strftime('%Y-%m', datetime(COALESCE({}, {})/1000, 'unixepoch')) = ?",
                      file_created_at_column, created_at_column));
      params.push_back(filters.month.value());
    }
  }

  if (filters.year.has_value()) {
    conditions.push_back(
        std::format("strftime('%Y', datetime(COALESCE({}, {})/1000, 'unixepoch')) = ?",
                    file_created_at_column, created_at_column));
    params.push_back(filters.year.value());
  }

  if (filters.type.has_value() && !filters.type->empty()) {
    conditions.push_back(type_column + " = ?");
    params.push_back(filters.type.value());
  }

  if (filters.search.has_value() && !filters.search->empty()) {
    conditions.push_back(name_column + " LIKE ?");
    params.push_back("%" + filters.search.value() + "%");
  }

  if (filters.rating.has_value()) {
    int rating = filters.rating.value();
    if (rating < 0 || rating > 5) {
      return std::unexpected("Rating filter must be between 0 and 5");
    }

    conditions.push_back(rating_column + " = ?");
    params.push_back(static_cast<std::int64_t>(rating));
  }

  if (filters.review_flag.has_value() && !filters.review_flag->empty()) {
    if (!is_valid_review_flag(filters.review_flag.value())) {
      return std::unexpected("Review flag filter must be one of none, picked, rejected");
    }

    conditions.push_back(review_flag_column + " = ?");
    params.push_back(filters.review_flag.value());
  }

  if (filters.tag_ids.has_value() && !filters.tag_ids->empty()) {
    std::string match_mode = filters.tag_match_mode.value_or("any");

    if (match_mode == "all") {
      for (const auto& tag_id : filters.tag_ids.value()) {
        conditions.push_back(
            std::format("{} IN (SELECT asset_id FROM asset_tags WHERE tag_id = ?)", id_column));
        params.push_back(tag_id);
      }
    } else {
      auto placeholders = build_in_clause_placeholders(filters.tag_ids->size());
      conditions.push_back(std::format(
          "{} IN (SELECT asset_id FROM asset_tags WHERE tag_id IN ({}))", id_column, placeholders));
      for (const auto& tag_id : filters.tag_ids.value()) {
        params.push_back(tag_id);
      }
    }
  }

  auto color_filter_result = Features::Gallery::Color::Filter::append_color_filter_conditions(
      filters, conditions, params, asset_table_alias);
  if (!color_filter_result) {
    return std::unexpected(color_filter_result.error());
  }

  std::string where_clause;
  if (!conditions.empty()) {
    where_clause =
        "WHERE " + std::ranges::fold_left(conditions, std::string{},
                                          [](const std::string& acc, const std::string& cond) {
                                            return acc.empty() ? cond : acc + " AND " + cond;
                                          });
  }

  return std::make_pair(where_clause, params);
}

auto find_active_asset_index(Core::State::AppState& app_state,
                             const Features::Gallery::Types::QueryAssetsFilters& filters,
                             const QueryOrderConfig& order_config, std::int64_t active_asset_id)
    -> std::expected<std::optional<std::int64_t>, std::string> {
  auto where_result = build_unified_where_clause(filters);
  if (!where_result) {
    return std::unexpected(where_result.error());
  }

  auto [where_clause, query_params] = std::move(where_result.value());

  std::string sql = std::format(R"(
    WITH filtered_assets AS (
      SELECT id,
             COALESCE(file_created_at, created_at) AS sort_created_at,
             file_created_at AS sort_file_created_at,
             name AS sort_name,
             (COALESCE(width, 0) * COALESCE(height, 0)) AS sort_resolution,
             COALESCE(width, 0) AS sort_width,
             COALESCE(height, 0) AS sort_height,
             size AS sort_size
      FROM assets
      {}
    ),
    indexed_assets AS (
      SELECT id,
             ROW_NUMBER() OVER ({}) - 1 AS row_index
      FROM filtered_assets
    )
    SELECT row_index
    FROM indexed_assets
    WHERE id = ?
  )",
                                where_clause, order_config.indexed_order_clause);

  query_params.push_back(active_asset_id);

  auto index_result =
      Core::Database::query_scalar<std::int64_t>(*app_state.database, sql, query_params);
  if (!index_result) {
    return std::unexpected("Failed to query active asset index: " + index_result.error());
  }

  return index_result.value();
}

}  // namespace Features::Gallery::Asset::QuerySupport
