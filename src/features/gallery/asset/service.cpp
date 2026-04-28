module;

module Features.Gallery.Asset.Service;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.OriginalLocator;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.InfinityNikkiMetadataDict;
import Features.Gallery.Color.Filter;
import Features.Gallery.Color.Repository;
import Extensions.InfinityNikki.WorldArea;
import Utils.Logger;
import <asio.hpp>;

namespace Features::Gallery::Asset::Service {

// ============= 内部辅助函数 =============

// 验证月份格式（YYYY-MM）
auto validate_month_format(const std::string& month) -> bool {
  if (month.length() != 7 || month[4] != '-') {
    return false;
  }

  // 简单验证：前4位和后2位是否为数字
  for (size_t i = 0; i < month.length(); ++i) {
    if (i == 4) continue;  // 跳过 '-'
    if (!std::isdigit(static_cast<unsigned char>(month[i]))) {
      return false;
    }
  }

  // 验证月份范围 01-12
  int month_num = std::stoi(month.substr(5, 2));
  return month_num >= 1 && month_num <= 12;
}

// 将时间戳转换为月份字符串
auto timestamp_to_month(std::int64_t timestamp_ms) -> std::string {
  // 将毫秒时间戳转换为 system_clock::time_point
  auto time_point = std::chrono::system_clock::time_point{std::chrono::milliseconds{timestamp_ms}};

  // 转换为 year_month_day
  auto ymd = std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(time_point)};

  // 格式化为 YYYY-MM
  return std::format("{:%Y-%m}", ymd);
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

auto trim_ascii_copy(std::string_view value) -> std::string {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

  std::size_t start = 0;
  while (start < value.size() && is_space(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && is_space(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

auto normalize_world_id_copy(std::string_view value) -> std::string {
  return Extensions::InfinityNikki::WorldArea::normalize_world_id(value);
}

struct PhotoMapPointWithWorldRecord {
  std::int64_t asset_id;
  std::string name;
  std::optional<std::string> hash;
  std::optional<std::int64_t> file_created_at;
  double nikki_loc_x;
  double nikki_loc_y;
  std::optional<double> nikki_loc_z;
  std::int64_t asset_index;
  std::optional<std::string> user_world_id;
};

struct InfinityNikkiUserRecordRow {
  std::string record_key;
  std::string record_value;
};

struct SameOutfitDyeCodeFillStatsRow {
  std::int64_t matched_count = 0;
  std::int64_t fillable_count = 0;
  std::int64_t recorded_count = 0;
};

struct InfinityNikkiSourceOutfitDyeStateRow {
  std::optional<std::string> nikki_diy_json;
  std::int64_t clothes_count = 0;
};

struct AssetIdRow {
  std::int64_t asset_id = 0;
};

auto read_infinity_nikki_user_record(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<Types::InfinityNikkiUserRecord, std::string> {
  auto rows_result = Core::Database::query<InfinityNikkiUserRecordRow>(*app_state.database,
                                                                       R"(
        SELECT record_key,
               record_value
        FROM asset_infinity_nikki_user_record
        WHERE asset_id = ?
      )",
                                                                       {asset_id});
  if (!rows_result) {
    return std::unexpected("Failed to query Infinity Nikki user record: " + rows_result.error());
  }

  Types::InfinityNikkiUserRecord record;
  for (const auto& row : rows_result.value()) {
    if (row.record_key == "dye_code") {
      record.dye_code = row.record_value;
    } else if (row.record_key == "home_building_code") {
      record.home_building_code = row.record_value;
    } else if (row.record_key == "world_id") {
      record.world_id = normalize_world_id_copy(row.record_value);
    }
  }

  return record;
}

auto has_infinity_nikki_user_record_value(const Types::InfinityNikkiUserRecord& record) -> bool {
  return record.dye_code.has_value() || record.home_building_code.has_value() ||
         record.world_id.has_value();
}

auto user_record_key_for_code_type(std::string_view code_type) -> std::optional<std::string> {
  if (code_type == "dye") {
    return std::string("dye_code");
  }
  if (code_type == "home_building") {
    return std::string("home_building_code");
  }
  return std::nullopt;
}

auto build_infinity_nikki_map_area(
    const std::optional<Types::InfinityNikkiExtractedParams>& extracted,
    const Types::InfinityNikkiUserRecord& user_record)
    -> std::optional<Types::InfinityNikkiMapArea> {
  if (!extracted.has_value() || !extracted->nikki_loc_x.has_value() ||
      !extracted->nikki_loc_y.has_value()) {
    return std::nullopt;
  }

  const Extensions::InfinityNikki::WorldArea::GamePoint game_point{
      .x = *extracted->nikki_loc_x,
      .y = *extracted->nikki_loc_y,
      .z = extracted->nikki_loc_z,
  };
  const auto auto_world_id =
      Extensions::InfinityNikki::WorldArea::resolve_world_id_or_default(game_point);
  const auto user_world_id =
      user_record.world_id.has_value()
          ? std::optional<std::string>{normalize_world_id_copy(*user_record.world_id)}
          : std::nullopt;

  return Types::InfinityNikkiMapArea{
      .auto_world_id = auto_world_id,
      .user_world_id = user_world_id,
      .world_id = user_world_id.value_or(auto_world_id),
  };
}

auto upsert_infinity_nikki_user_record_key(Core::State::AppState& app_state, std::int64_t asset_id,
                                           std::string_view record_key,
                                           std::string_view record_value)
    -> std::expected<void, std::string> {
  auto result =
      Core::Database::execute(*app_state.database,
                              R"(
      INSERT INTO asset_infinity_nikki_user_record (asset_id, record_key, record_value)
      VALUES (?, ?, ?)
      ON CONFLICT(asset_id, record_key) DO UPDATE SET
        record_value = excluded.record_value
    )",
                              {asset_id, std::string(record_key), std::string(record_value)});
  if (!result) {
    return std::unexpected("Failed to upsert Infinity Nikki user record key: " + result.error());
  }
  return {};
}

auto delete_infinity_nikki_user_record_keys(Core::State::AppState& app_state, std::int64_t asset_id,
                                            const std::vector<std::string>& record_keys)
    -> std::expected<void, std::string> {
  for (const auto& record_key : record_keys) {
    auto result = Core::Database::execute(
        *app_state.database,
        "DELETE FROM asset_infinity_nikki_user_record WHERE asset_id = ? AND record_key = ?",
        {asset_id, record_key});
    if (!result) {
      return std::unexpected("Failed to delete Infinity Nikki user record key: " + result.error());
    }
  }
  return {};
}

auto db_param_from_optional_string(const std::optional<std::string>& value)
    -> Core::Database::Types::DbParam {
  if (!value.has_value()) {
    return std::monostate{};
  }
  return *value;
}

auto read_infinity_nikki_source_outfit_dye_state(Core::State::AppState& app_state,
                                                 std::int64_t asset_id)
    -> std::expected<std::optional<InfinityNikkiSourceOutfitDyeStateRow>, std::string> {
  auto result =
      Core::Database::query_single<InfinityNikkiSourceOutfitDyeStateRow>(*app_state.database,
                                                                         R"(
        SELECT p.nikki_diy_json AS nikki_diy_json,
               (
                 SELECT COUNT(*)
                 FROM asset_infinity_nikki_clothes c
                 WHERE c.asset_id = p.asset_id
               ) AS clothes_count
        FROM asset_infinity_nikki_params p
        WHERE p.asset_id = ?
      )",
                                                                         {asset_id});
  if (!result) {
    return std::unexpected("Failed to query Infinity Nikki outfit and dye state: " +
                           result.error());
  }

  return result.value();
}

auto query_same_outfit_dye_code_fill_preview(
    Core::State::AppState& app_state, std::int64_t source_asset_id,
    const InfinityNikkiSourceOutfitDyeStateRow& source_state)
    -> std::expected<Types::InfinityNikkiSameOutfitDyeCodeFillPreview, std::string> {
  const auto diy_json_param = db_param_from_optional_string(source_state.nikki_diy_json);
  auto stats_result = Core::Database::query_single<SameOutfitDyeCodeFillStatsRow>(
      *app_state.database,
      R"(
        SELECT COUNT(*) AS matched_count,
               COALESCE(SUM(
                 CASE
                   WHEN ur.asset_id IS NULL OR trim(ur.record_value) = '' THEN 1
                   ELSE 0
                 END
               ), 0) AS fillable_count,
               COALESCE(SUM(
                 CASE
                   WHEN ur.asset_id IS NOT NULL AND trim(ur.record_value) <> '' THEN 1
                   ELSE 0
                 END
               ), 0) AS recorded_count
        FROM asset_infinity_nikki_params p
        LEFT JOIN asset_infinity_nikki_user_record ur
          ON ur.asset_id = p.asset_id
         AND ur.record_key = 'dye_code'
        WHERE p.asset_id <> ?
          AND ((? IS NULL AND p.nikki_diy_json IS NULL) OR p.nikki_diy_json = ?)
          AND (
            SELECT COUNT(*)
            FROM asset_infinity_nikki_clothes c
            WHERE c.asset_id = p.asset_id
          ) = ?
          AND NOT EXISTS (
            SELECT 1
            FROM asset_infinity_nikki_clothes sc
            WHERE sc.asset_id = ?
              AND NOT EXISTS (
                SELECT 1
                FROM asset_infinity_nikki_clothes cc
                WHERE cc.asset_id = p.asset_id
                  AND cc.cloth_id = sc.cloth_id
              )
          )
      )",
      {source_asset_id, diy_json_param, diy_json_param, source_state.clothes_count,
       source_asset_id});
  if (!stats_result) {
    return std::unexpected("Failed to count same Infinity Nikki outfit and dye assets: " +
                           stats_result.error());
  }

  const auto stats = stats_result->value_or(SameOutfitDyeCodeFillStatsRow{});
  return Types::InfinityNikkiSameOutfitDyeCodeFillPreview{
      .source_has_outfit_dye_state = true,
      .matched_count = stats.matched_count,
      .fillable_count = stats.fillable_count,
      .recorded_count = stats.recorded_count,
  };
}

auto query_same_outfit_dye_code_fillable_asset_ids(
    Core::State::AppState& app_state, std::int64_t source_asset_id,
    const InfinityNikkiSourceOutfitDyeStateRow& source_state)
    -> std::expected<std::vector<std::int64_t>, std::string> {
  const auto diy_json_param = db_param_from_optional_string(source_state.nikki_diy_json);
  auto rows_result =
      Core::Database::query<AssetIdRow>(*app_state.database,
                                        R"(
        SELECT p.asset_id AS asset_id
        FROM asset_infinity_nikki_params p
        LEFT JOIN asset_infinity_nikki_user_record ur
          ON ur.asset_id = p.asset_id
         AND ur.record_key = 'dye_code'
        WHERE p.asset_id <> ?
          AND ((? IS NULL AND p.nikki_diy_json IS NULL) OR p.nikki_diy_json = ?)
          AND (
            SELECT COUNT(*)
            FROM asset_infinity_nikki_clothes c
            WHERE c.asset_id = p.asset_id
          ) = ?
          AND NOT EXISTS (
            SELECT 1
            FROM asset_infinity_nikki_clothes sc
            WHERE sc.asset_id = ?
              AND NOT EXISTS (
                SELECT 1
                FROM asset_infinity_nikki_clothes cc
                WHERE cc.asset_id = p.asset_id
                  AND cc.cloth_id = sc.cloth_id
              )
          )
          AND (ur.asset_id IS NULL OR trim(ur.record_value) = '')
      )",
                                        {source_asset_id, diy_json_param, diy_json_param,
                                         source_state.clothes_count, source_asset_id});
  if (!rows_result) {
    return std::unexpected("Failed to query same Infinity Nikki outfit and dye assets: " +
                           rows_result.error());
  }

  std::vector<std::int64_t> asset_ids;
  asset_ids.reserve(rows_result->size());
  for (const auto& row : rows_result.value()) {
    asset_ids.push_back(row.asset_id);
  }
  return asset_ids;
}

auto query_same_outfit_dye_code_target_asset_ids(
    Core::State::AppState& app_state, std::int64_t source_asset_id,
    const InfinityNikkiSourceOutfitDyeStateRow& source_state)
    -> std::expected<std::vector<std::int64_t>, std::string> {
  const auto diy_json_param = db_param_from_optional_string(source_state.nikki_diy_json);
  auto rows_result =
      Core::Database::query<AssetIdRow>(*app_state.database,
                                        R"(
        SELECT p.asset_id AS asset_id
        FROM asset_infinity_nikki_params p
        WHERE p.asset_id <> ?
          AND ((? IS NULL AND p.nikki_diy_json IS NULL) OR p.nikki_diy_json = ?)
          AND (
            SELECT COUNT(*)
            FROM asset_infinity_nikki_clothes c
            WHERE c.asset_id = p.asset_id
          ) = ?
          AND NOT EXISTS (
            SELECT 1
            FROM asset_infinity_nikki_clothes sc
            WHERE sc.asset_id = ?
              AND NOT EXISTS (
                SELECT 1
                FROM asset_infinity_nikki_clothes cc
                WHERE cc.asset_id = p.asset_id
                  AND cc.cloth_id = sc.cloth_id
              )
          )
      )",
                                        {source_asset_id, diy_json_param, diy_json_param,
                                         source_state.clothes_count, source_asset_id});
  if (!rows_result) {
    return std::unexpected("Failed to query same Infinity Nikki outfit and dye assets: " +
                           rows_result.error());
  }

  std::vector<std::int64_t> asset_ids;
  asset_ids.reserve(rows_result->size());
  for (const auto& row : rows_result.value()) {
    asset_ids.push_back(row.asset_id);
  }
  return asset_ids;
}

auto build_unified_where_clause(const Types::QueryAssetsFilters& filters,
                                std::string_view asset_table_alias = "")
    -> std::expected<std::pair<std::string, std::vector<Core::Database::Types::DbParam>>,
                     std::string>;

struct QueryOrderConfig {
  std::string sort_by;
  std::string sort_order;
  std::string asset_order_clause;
  // 给 ROW_NUMBER() 用的排序子句；需要基于中间列名而不是 assets 原始表达式。
  std::string indexed_order_clause;
};

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

auto find_active_asset_index(Core::State::AppState& app_state,
                             const Types::QueryAssetsFilters& filters,
                             const QueryOrderConfig& order_config, std::int64_t active_asset_id)
    -> std::expected<std::optional<std::int64_t>, std::string> {
  // 复用与主查询完全一致的筛选/排序语义，只额外回答“当前 active 资产在第几位”。
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

// 构建统一的WHERE条件
auto build_unified_where_clause(const Types::QueryAssetsFilters& filters,
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

  // 文件夹筛选
  if (filters.folder_id.has_value()) {
    if (filters.include_subfolders.value_or(false)) {
      // 使用递归CTE查询当前文件夹及所有子文件夹
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
      // 只查询当前文件夹
      conditions.push_back(folder_id_column + " = ?");
      params.push_back(filters.folder_id.value());
    }
  }

  // 月份筛选
  if (filters.month.has_value()) {
    if (!validate_month_format(filters.month.value())) {
      // 注意：这里如果格式不对，我们就忽略这个条件
      // 或者可以在上层进行验证
    } else {
      conditions.push_back(
          std::format("strftime('%Y-%m', datetime(COALESCE({}, {})/1000, 'unixepoch')) = ?",
                      file_created_at_column, created_at_column));
      params.push_back(filters.month.value());
    }
  }

  // 年份筛选
  if (filters.year.has_value()) {
    conditions.push_back(
        std::format("strftime('%Y', datetime(COALESCE({}, {})/1000, 'unixepoch')) = ?",
                    file_created_at_column, created_at_column));
    params.push_back(filters.year.value());
  }

  // 类型筛选
  if (filters.type.has_value() && !filters.type->empty()) {
    conditions.push_back(type_column + " = ?");
    params.push_back(filters.type.value());
  }

  // 搜索
  if (filters.search.has_value() && !filters.search->empty()) {
    conditions.push_back(name_column + " LIKE ?");
    params.push_back("%" + filters.search.value() + "%");
  }

  // 审片筛选：评分和留用/弃置都属于资产固有元数据，因此直接挂在 assets 表上筛选。
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

  // 标签筛选
  if (filters.tag_ids.has_value() && !filters.tag_ids->empty()) {
    std::string match_mode = filters.tag_match_mode.value_or("any");

    if (match_mode == "all") {
      // 匹配所有标签（AND）：资产必须拥有所有指定的标签
      for (const auto& tag_id : filters.tag_ids.value()) {
        conditions.push_back(
            std::format("{} IN (SELECT asset_id FROM asset_tags WHERE tag_id = ?)", id_column));
        params.push_back(tag_id);
      }
    } else {
      // 匹配任一标签（OR）：资产拥有任意一个标签即可
      auto placeholders = build_in_clause_placeholders(filters.tag_ids->size());
      conditions.push_back(std::format(
          "{} IN (SELECT asset_id FROM asset_tags WHERE tag_id IN ({}))", id_column, placeholders));
      for (const auto& tag_id : filters.tag_ids.value()) {
        params.push_back(tag_id);
      }
    }
  }

  // 无限暖暖服装筛选
  if (filters.cloth_ids.has_value() && !filters.cloth_ids->empty()) {
    std::string match_mode = filters.cloth_match_mode.value_or("any");
    auto placeholders = build_in_clause_placeholders(filters.cloth_ids->size());

    if (match_mode == "all") {
      conditions.push_back(std::format(
          R"({} IN (
            SELECT asset_id
            FROM asset_infinity_nikki_clothes
            WHERE cloth_id IN ({})
            GROUP BY asset_id
            HAVING COUNT(DISTINCT cloth_id) = ?
          ))",
          id_column, placeholders));

      for (const auto cloth_id : filters.cloth_ids.value()) {
        params.push_back(cloth_id);
      }
      params.push_back(static_cast<std::int64_t>(filters.cloth_ids->size()));
    } else {
      conditions.push_back(std::format(
          "{} IN (SELECT DISTINCT asset_id FROM asset_infinity_nikki_clothes WHERE cloth_id IN "
          "({}))",
          id_column, placeholders));
      for (const auto cloth_id : filters.cloth_ids.value()) {
        params.push_back(cloth_id);
      }
    }
  }

  auto color_filter_result = Features::Gallery::Color::Filter::append_color_filter_conditions(
      filters, conditions, params, asset_table_alias);
  if (!color_filter_result) {
    return std::unexpected(color_filter_result.error());
  }

  // 构建 WHERE 子句
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

// ============= 查询服务实现 =============

// 统一的资产查询函数
auto query_assets(Core::State::AppState& app_state, const Types::QueryAssetsParams& params)
    -> std::expected<Types::ListResponse, std::string> {
  // 1. 构建通用WHERE条件
  auto where_result = build_unified_where_clause(params.filters);
  if (!where_result) {
    return std::unexpected(where_result.error());
  }
  auto [where_clause, where_params] = std::move(where_result.value());

  // 2. 验证和构建 ORDER BY
  auto order_config = build_query_order_config(params.sort_by, params.sort_order);

  // 3. 获取总数（用于分页计算或前端显示）
  std::string count_sql = std::format("SELECT COUNT(*) FROM assets {}", where_clause);
  auto total_count_result =
      Core::Database::query_scalar<int>(*app_state.database, count_sql, where_params);
  if (!total_count_result) {
    return std::unexpected("Failed to count assets: " + total_count_result.error());
  }
  int total_count = total_count_result->value_or(0);

  // 4. 构建主查询
  std::string sql = std::format(R"(
    SELECT id, name, path, type,
           (
             SELECT printf('#%02X%02X%02X', ac.r, ac.g, ac.b)
             FROM asset_colors ac
             WHERE ac.asset_id = assets.id
             ORDER BY ac.weight DESC, ac.id ASC
             LIMIT 1
           ) AS dominant_color_hex,
           rating, review_flag,
           description, width, height, size, extension, mime_type, hash,
           NULL AS root_id, NULL AS relative_path, folder_id,
           file_created_at, file_modified_at,
           created_at, updated_at
    FROM assets
    {}
    {}
  )",
                                where_clause, order_config.asset_order_clause);

  auto final_params = where_params;

  // 5. 如果需要分页，添加 LIMIT/OFFSET
  int page = 1;
  int per_page = 50;
  bool has_pagination = params.page.has_value() && params.per_page.has_value();

  if (has_pagination) {
    page = params.page.value();
    per_page = params.per_page.value();
    int offset = (page - 1) * per_page;
    sql += " LIMIT ? OFFSET ?";
    final_params.push_back(per_page);
    final_params.push_back(offset);
  }

  // 6. 执行查询
  auto assets_result = Core::Database::query<Types::Asset>(*app_state.database, sql, final_params);
  if (!assets_result) {
    return std::unexpected("Failed to query assets: " + assets_result.error());
  }

  // 7. 构建响应
  Types::ListResponse response;
  response.items = std::move(assets_result.value());

  auto locator_result =
      Features::Gallery::OriginalLocator::populate_asset_locators(app_state, response.items);
  if (!locator_result) {
    return std::unexpected(locator_result.error());
  }

  response.total_count = total_count;

  if (has_pagination) {
    response.current_page = page;
    response.per_page = per_page;
    response.total_pages = (total_count + per_page - 1) / per_page;
  } else {
    // 不分页时，返回简单的分页信息
    response.current_page = 1;
    response.per_page = total_count;
    response.total_pages = 1;
  }

  if (params.active_asset_id.has_value()) {
    auto active_index_result = find_active_asset_index(app_state, params.filters, order_config,
                                                       params.active_asset_id.value());
    if (!active_index_result) {
      return std::unexpected(active_index_result.error());
    }
    response.active_asset_index = active_index_result.value();
  }

  return response;
}

auto query_asset_layout_meta(Core::State::AppState& app_state,
                             const Types::QueryAssetLayoutMetaParams& params)
    -> std::expected<Types::QueryAssetLayoutMetaResponse, std::string> {
  auto where_result = build_unified_where_clause(params.filters);
  if (!where_result) {
    return std::unexpected(where_result.error());
  }
  auto [where_clause, where_params] = std::move(where_result.value());

  auto order_config = build_query_order_config(params.sort_by, params.sort_order);

  std::string count_sql = std::format("SELECT COUNT(*) FROM assets {}", where_clause);
  auto total_count_result =
      Core::Database::query_scalar<int>(*app_state.database, count_sql, where_params);
  if (!total_count_result) {
    return std::unexpected("Failed to count assets for layout meta: " + total_count_result.error());
  }

  std::string sql = std::format(R"(
    SELECT id, width, height
    FROM assets
    {}
    {}
  )",
                                where_clause, order_config.asset_order_clause);

  auto items_result =
      Core::Database::query<Types::AssetLayoutMetaItem>(*app_state.database, sql, where_params);
  if (!items_result) {
    return std::unexpected("Failed to query asset layout meta: " + items_result.error());
  }

  Types::QueryAssetLayoutMetaResponse response;
  response.items = std::move(items_result.value());
  response.total_count = total_count_result->value_or(0);
  return response;
}

auto query_photo_map_points(Core::State::AppState& app_state,
                            const Types::QueryPhotoMapPointsParams& params)
    -> std::expected<std::vector<Types::PhotoMapPoint>, std::string> {
  // 目标：为每个 marker 计算它在“当前 gallery 排序结果集”的 index，
  // 以便前端打开灯箱时 activeIndex 一次性对齐，避免闪一下。

  // 1) 复用 gallery 的“查询过滤语义”，计算排序 index 时不要提前丢掉视频等其它类型。
  auto where_result = build_unified_where_clause(params.filters, "a");
  if (!where_result) {
    return std::unexpected(where_result.error());
  }
  auto [where_clause, query_params] = std::move(where_result.value());

  // 2) 复用 gallery 的排序语义（sort_by/sort_order）。
  auto order_config = build_query_order_config(params.sort_by, params.sort_order);

  // 3) 在全量 filtered_assets 上计算 asset_index，再 JOIN Infinity Nikki 坐标表并过滤出可渲染
  // marker。
  //    注意：asset_index 是在“全量 filtered_assets 排序结果”里的下标；最终 WHERE 只影响 marker
  //    子集输出， 不改变 asset_index 的数值。
  std::string sql = std::format(R"(
    WITH filtered_assets AS (
      SELECT a.id,
             a.name,
             a.hash,
             a.file_created_at,
             a.type AS asset_type,

             COALESCE(a.file_created_at, a.created_at) AS sort_created_at,
             a.file_created_at AS sort_file_created_at,
             a.name AS sort_name,
             (COALESCE(a.width, 0) * COALESCE(a.height, 0)) AS sort_resolution,
             COALESCE(a.width, 0) AS sort_width,
             COALESCE(a.height, 0) AS sort_height,
             a.size AS sort_size
      FROM assets a
      {}
    ),
    indexed_assets AS (
      SELECT id,
             ROW_NUMBER() OVER ({}) - 1 AS asset_index
      FROM filtered_assets
    )
    SELECT fa.id AS asset_id,
           fa.name,
           fa.hash,
           fa.file_created_at,
           p.nikki_loc_x,
           p.nikki_loc_y,
           p.nikki_loc_z,
           ia.asset_index AS asset_index,
           wr.record_value AS user_world_id
    FROM indexed_assets ia
    INNER JOIN filtered_assets fa ON fa.id = ia.id
    INNER JOIN asset_infinity_nikki_params p ON p.asset_id = ia.id
    LEFT JOIN asset_infinity_nikki_user_record wr
      ON wr.asset_id = ia.id
     AND wr.record_key = 'world_id'
    WHERE p.nikki_loc_x IS NOT NULL
      AND p.nikki_loc_y IS NOT NULL
      AND fa.asset_type IN ('photo', 'live_photo')
    ORDER BY ia.asset_index
  )",
                                where_clause, order_config.indexed_order_clause);

  auto result =
      Core::Database::query<PhotoMapPointWithWorldRecord>(*app_state.database, sql, query_params);
  if (!result) {
    return std::unexpected("Failed to query photo map points: " + result.error());
  }

  const auto world_id = normalize_world_id_copy(params.world_id);
  if (world_id.empty()) {
    return std::vector<Types::PhotoMapPoint>{};
  }

  std::vector<Types::PhotoMapPoint> filtered_points;
  filtered_points.reserve(result->size());

  for (const auto& point : result.value()) {
    const Extensions::InfinityNikki::WorldArea::GamePoint game_point{
        .x = point.nikki_loc_x,
        .y = point.nikki_loc_y,
        .z = point.nikki_loc_z,
    };
    const auto resolved_world_id =
        point.user_world_id.has_value()
            ? normalize_world_id_copy(*point.user_world_id)
            : Extensions::InfinityNikki::WorldArea::resolve_world_id_or_default(game_point);
    if (resolved_world_id != world_id) {
      continue;
    }
    filtered_points.push_back(Types::PhotoMapPoint{
        .asset_id = point.asset_id,
        .name = point.name,
        .hash = point.hash,
        .file_created_at = point.file_created_at,
        .nikki_loc_x = point.nikki_loc_x,
        .nikki_loc_y = point.nikki_loc_y,
        .nikki_loc_z = point.nikki_loc_z,
        .asset_index = point.asset_index,
    });
  }

  return filtered_points;
}

auto get_timeline_buckets(Core::State::AppState& app_state,
                          const Types::TimelineBucketsParams& params)
    -> std::expected<Types::TimelineBucketsResponse, std::string> {
  // 将 TimelineBucketsParams 转换为 QueryAssetsFilters，复用统一的过滤逻辑
  Types::QueryAssetsFilters filters;
  filters.folder_id = params.folder_id;
  filters.include_subfolders = params.include_subfolders;
  filters.type = params.type;
  filters.search = params.search;
  filters.rating = params.rating;
  filters.review_flag = params.review_flag;
  filters.tag_ids = params.tag_ids;
  filters.tag_match_mode = params.tag_match_mode;
  filters.cloth_ids = params.cloth_ids;
  filters.cloth_match_mode = params.cloth_match_mode;
  filters.color_hexes = params.color_hexes;
  filters.color_match_mode = params.color_match_mode;
  filters.color_distance = params.color_distance;

  auto order_config =
      build_query_order_config(std::optional<std::string>{"created_at"}, params.sort_order);

  // 复用统一的 WHERE 条件构建器
  auto where_result = build_unified_where_clause(filters);
  if (!where_result) {
    return std::unexpected(where_result.error());
  }
  auto [where_clause, query_params] = std::move(where_result.value());

  // 构建查询
  std::string sql = std::format(R"(
    SELECT 
      strftime('%Y-%m', datetime(COALESCE(file_created_at, created_at)/1000, 'unixepoch')) as month,
      COUNT(*) as count
    FROM assets 
    {}
    GROUP BY month
    ORDER BY month {}
  )",
                                where_clause, order_config.sort_order);

  auto result =
      Core::Database::query<Types::TimelineBucket>(*app_state.database, sql, query_params);

  if (!result) {
    return std::unexpected("Failed to query timeline buckets: " + result.error());
  }

  // 计算总数
  int total_count = 0;
  for (const auto& bucket : result.value()) {
    total_count += bucket.count;
  }

  Types::TimelineBucketsResponse response;
  response.buckets = std::move(result.value());
  response.total_count = total_count;

  if (params.active_asset_id.has_value()) {
    auto active_index_result =
        find_active_asset_index(app_state, filters, order_config, params.active_asset_id.value());
    if (!active_index_result) {
      return std::unexpected(active_index_result.error());
    }
    response.active_asset_index = active_index_result.value();
  }

  return response;
}

auto get_assets_by_month(Core::State::AppState& app_state,
                         const Types::GetAssetsByMonthParams& params)
    -> std::expected<Types::GetAssetsByMonthResponse, std::string> {
  // 验证月份格式
  if (!validate_month_format(params.month)) {
    return std::unexpected("Invalid month format. Expected: YYYY-MM");
  }

  // 转换为统一查询参数
  Types::QueryAssetsParams query_params;
  query_params.filters.folder_id = params.folder_id;
  query_params.filters.include_subfolders = params.include_subfolders;
  query_params.filters.month = params.month;  // 关键：月份变成筛选条件
  query_params.filters.type = params.type;
  query_params.filters.search = params.search;
  query_params.filters.rating = params.rating;
  query_params.filters.review_flag = params.review_flag;
  query_params.filters.tag_ids = params.tag_ids;
  query_params.filters.tag_match_mode = params.tag_match_mode;
  query_params.filters.cloth_ids = params.cloth_ids;
  query_params.filters.cloth_match_mode = params.cloth_match_mode;
  query_params.filters.color_hexes = params.color_hexes;
  query_params.filters.color_match_mode = params.color_match_mode;
  query_params.filters.color_distance = params.color_distance;
  query_params.sort_by = "created_at";
  query_params.sort_order = params.sort_order;
  // 注意：不传 page，所以返回该月全部数据

  // 调用统一查询接口
  auto result = query_assets(app_state, query_params);
  if (!result) {
    return std::unexpected(result.error());
  }

  // 转换为 GetAssetsByMonthResponse 格式
  Types::GetAssetsByMonthResponse response;
  response.month = params.month;
  response.assets = std::move(result.value().items);
  response.count = static_cast<int>(response.assets.size());

  return response;
}

auto get_infinity_nikki_details(Core::State::AppState& app_state,
                                const Types::GetInfinityNikkiDetailsParams& params)
    -> std::expected<Types::InfinityNikkiDetails, std::string> {
  std::string extracted_sql = R"(
    SELECT camera_params,
           time_hour,
           time_min,
           camera_focal_length,
           rotation,
           aperture_value,
           filter_id,
           filter_strength,
           vignette_intensity,
           light_id,
           light_strength,
           vertical,
           bloom_intensity,
           bloom_threshold,
           brightness,
           exposure,
           contrast,
           saturation,
           vibrance,
           highlights,
           shadow,
           nikki_loc_x,
           nikki_loc_y,
           nikki_loc_z,
           nikki_hidden,
           pose_id
    FROM asset_infinity_nikki_params
    WHERE asset_id = ?
  )";

  auto extracted_result = Core::Database::query_single<Types::InfinityNikkiExtractedParams>(
      *app_state.database, extracted_sql, {params.asset_id});
  if (!extracted_result) {
    return std::unexpected("Failed to query Infinity Nikki extracted params: " +
                           extracted_result.error());
  }

  auto user_record_result = read_infinity_nikki_user_record(app_state, params.asset_id);
  if (!user_record_result) {
    return std::unexpected(user_record_result.error());
  }

  auto user_record = std::move(user_record_result.value());
  auto map_area = build_infinity_nikki_map_area(extracted_result.value(), user_record);
  auto user_record_optional = has_infinity_nikki_user_record_value(user_record)
                                  ? std::optional<Types::InfinityNikkiUserRecord>{user_record}
                                  : std::nullopt;

  return Types::InfinityNikkiDetails{.extracted = extracted_result.value(),
                                     .user_record = std::move(user_record_optional),
                                     .map_area = std::move(map_area)};
}

auto get_asset_main_colors(Core::State::AppState& app_state,
                           const Types::GetAssetMainColorsParams& params)
    -> std::expected<std::vector<Types::AssetMainColor>, std::string> {
  auto result =
      Features::Gallery::Color::Repository::get_asset_main_colors(app_state, params.asset_id);
  if (!result) {
    return std::unexpected(result.error());
  }

  return result.value();
}

auto get_home_stats(Core::State::AppState& app_state)
    -> std::expected<Types::HomeStats, std::string> {
  std::string sql = R"(
    SELECT
      COUNT(*) AS total_count,
      COALESCE(SUM(CASE WHEN type = 'photo' THEN 1 ELSE 0 END), 0) AS photo_count,
      COALESCE(SUM(CASE WHEN type = 'video' THEN 1 ELSE 0 END), 0) AS video_count,
      COALESCE(SUM(CASE WHEN type = 'live_photo' THEN 1 ELSE 0 END), 0) AS live_photo_count,
      COALESCE(SUM(CASE WHEN size IS NOT NULL AND size > 0 THEN size ELSE 0 END), 0) AS total_size,
      COALESCE(
        SUM(
          CASE
            WHEN date(datetime(COALESCE(file_created_at, created_at) / 1000, 'unixepoch', 'localtime')) =
                 date('now', 'localtime') THEN 1
            ELSE 0
          END
        ),
        0
      ) AS today_added_count
    FROM assets
  )";

  auto result = Core::Database::query_single<Types::HomeStats>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to query home stats: " + result.error());
  }

  return result.value().value_or(Types::HomeStats{});
}

auto update_assets_review_state(Core::State::AppState& app_state,
                                const Types::UpdateAssetsReviewStateParams& params)
    -> std::expected<Types::OperationResult, std::string> {
  if (params.asset_ids.empty()) {
    return std::unexpected("No assets selected for review update");
  }

  if (!params.rating.has_value() && !params.review_flag.has_value()) {
    return std::unexpected("At least one review field must be provided");
  }

  if (params.rating.has_value() && (params.rating.value() < 0 || params.rating.value() > 5)) {
    return std::unexpected("Rating must be between 0 and 5");
  }

  if (params.review_flag.has_value() && !is_valid_review_flag(params.review_flag.value())) {
    return std::unexpected("Review flag must be one of none, picked, rejected");
  }

  std::vector<std::string> set_clauses;
  std::vector<Core::Database::Types::DbParam> db_params;

  // 这里使用统一的批量更新入口，后续如果想扩展标记时间、操作者等元数据，只需在这里继续扩展。
  if (params.rating.has_value()) {
    set_clauses.push_back("rating = ?");
    db_params.push_back(static_cast<std::int64_t>(params.rating.value()));
  }

  if (params.review_flag.has_value()) {
    set_clauses.push_back("review_flag = ?");
    db_params.push_back(params.review_flag.value());
  }

  auto placeholders = build_in_clause_placeholders(params.asset_ids.size());
  std::string sql =
      std::format("UPDATE assets SET {} WHERE id IN ({})",
                  std::ranges::fold_left(set_clauses, std::string{},
                                         [](const std::string& acc, const std::string& clause) {
                                           return acc.empty() ? clause : acc + ", " + clause;
                                         }),
                  placeholders);

  for (const auto asset_id : params.asset_ids) {
    db_params.push_back(asset_id);
  }

  auto result = Core::Database::execute(*app_state.database, sql, db_params);
  if (!result) {
    return std::unexpected("Failed to update assets review state: " + result.error());
  }

  return Types::OperationResult{
      .success = true,
      .message = "Assets review state updated successfully",
      .affected_count = static_cast<std::int64_t>(params.asset_ids.size())};
}

auto update_asset_description(Core::State::AppState& app_state,
                              const Types::UpdateAssetDescriptionParams& params)
    -> std::expected<Types::OperationResult, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before updating description: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto normalized_description =
      params.description.has_value()
          ? std::optional<std::string>{trim_ascii_copy(params.description.value())}
          : std::nullopt;
  if (normalized_description.has_value() && normalized_description->empty()) {
    normalized_description = std::nullopt;
  }

  std::vector<Core::Database::Types::DbParam> db_params;
  db_params.push_back(normalized_description.has_value()
                          ? Core::Database::Types::DbParam{normalized_description.value()}
                          : Core::Database::Types::DbParam{std::monostate{}});
  db_params.push_back(params.asset_id);

  auto result = Core::Database::execute(
      *app_state.database, "UPDATE assets SET description = ? WHERE id = ?", db_params);
  if (!result) {
    return std::unexpected("Failed to update asset description: " + result.error());
  }

  auto affected_result =
      Core::Database::query_scalar<std::int64_t>(*app_state.database, "SELECT changes()");
  if (!affected_result) {
    return std::unexpected("Failed to query updated asset count: " + affected_result.error());
  }

  return Types::OperationResult{.success = true,
                                .message = "Asset description updated successfully",
                                .affected_count = affected_result->value_or(0)};
}

auto set_infinity_nikki_user_record(Core::State::AppState& app_state,
                                    const Types::SetInfinityNikkiUserRecordParams& params)
    -> std::expected<Types::OperationResult, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  if (params.code_type != "dye" && params.code_type != "home_building") {
    return std::unexpected("Infinity Nikki code type must be one of dye, home_building");
  }
  const auto record_key = user_record_key_for_code_type(params.code_type);
  if (!record_key.has_value()) {
    return std::unexpected("Unsupported Infinity Nikki code type");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before updating Infinity Nikki user record: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto normalized_code_value =
      params.code_value.has_value()
          ? std::optional<std::string>{trim_ascii_copy(params.code_value.value())}
          : std::nullopt;
  if (normalized_code_value.has_value() && normalized_code_value->empty()) {
    normalized_code_value = std::nullopt;
  }

  auto write_result = Core::Database::execute_transaction(
      *app_state.database, [&](auto&) -> std::expected<void, std::string> {
        if (normalized_code_value.has_value()) {
          auto value_result = upsert_infinity_nikki_user_record_key(
              app_state, params.asset_id, *record_key, normalized_code_value.value());
          if (!value_result) {
            return std::unexpected(value_result.error());
          }
          return {};
        }

        return delete_infinity_nikki_user_record_keys(app_state, params.asset_id, {*record_key});
      });

  if (!write_result) {
    return std::unexpected(write_result.error());
  }

  return Types::OperationResult{.success = true,
                                .message = "Infinity Nikki user record updated successfully",
                                .affected_count = 1};
}

auto preview_infinity_nikki_same_outfit_dye_code_fill(
    Core::State::AppState& app_state,
    const Types::PreviewInfinityNikkiSameOutfitDyeCodeFillParams& params)
    -> std::expected<Types::InfinityNikkiSameOutfitDyeCodeFillPreview, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before previewing same outfit and dye fill: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto source_state_result =
      read_infinity_nikki_source_outfit_dye_state(app_state, params.asset_id);
  if (!source_state_result) {
    return std::unexpected(source_state_result.error());
  }

  if (!source_state_result->has_value()) {
    return Types::InfinityNikkiSameOutfitDyeCodeFillPreview{};
  }

  const auto& source_state = source_state_result->value();
  if (source_state.clothes_count <= 0) {
    return Types::InfinityNikkiSameOutfitDyeCodeFillPreview{};
  }

  return query_same_outfit_dye_code_fill_preview(app_state, params.asset_id, source_state);
}

auto fill_infinity_nikki_same_outfit_dye_code(
    Core::State::AppState& app_state, const Types::FillInfinityNikkiSameOutfitDyeCodeParams& params)
    -> std::expected<Types::InfinityNikkiSameOutfitDyeCodeFillResult, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  auto normalized_code_value = trim_ascii_copy(params.code_value);
  if (normalized_code_value.empty()) {
    return std::unexpected("Dye code must not be empty");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before filling same outfit and dye records: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto source_state_result =
      read_infinity_nikki_source_outfit_dye_state(app_state, params.asset_id);
  if (!source_state_result) {
    return std::unexpected(source_state_result.error());
  }

  if (!source_state_result->has_value()) {
    return Types::InfinityNikkiSameOutfitDyeCodeFillResult{
        .success = true,
        .message = "Source asset has no Infinity Nikki outfit data",
        .source_has_outfit_dye_state = false,
    };
  }

  const auto& source_state = source_state_result->value();
  if (source_state.clothes_count <= 0) {
    return Types::InfinityNikkiSameOutfitDyeCodeFillResult{
        .success = true,
        .message = "Source asset has no Infinity Nikki outfit data",
        .source_has_outfit_dye_state = false,
    };
  }

  auto preview_result =
      query_same_outfit_dye_code_fill_preview(app_state, params.asset_id, source_state);
  if (!preview_result) {
    return std::unexpected(preview_result.error());
  }

  auto target_ids_result =
      query_same_outfit_dye_code_target_asset_ids(app_state, params.asset_id, source_state);
  if (!target_ids_result) {
    return std::unexpected(target_ids_result.error());
  }

  auto target_ids = std::move(target_ids_result.value());
  auto write_result = Core::Database::execute_transaction(
      *app_state.database, [&](auto&) -> std::expected<void, std::string> {
        for (const auto asset_id : target_ids) {
          auto value_result = upsert_infinity_nikki_user_record_key(app_state, asset_id, "dye_code",
                                                                    normalized_code_value);
          if (!value_result) {
            return std::unexpected(value_result.error());
          }
        }
        return {};
      });

  if (!write_result) {
    return std::unexpected(write_result.error());
  }

  return Types::InfinityNikkiSameOutfitDyeCodeFillResult{
      .success = true,
      .message = "Infinity Nikki same outfit and dye records filled successfully",
      .source_has_outfit_dye_state = true,
      .matched_count = preview_result->matched_count,
      .affected_count = static_cast<std::int64_t>(target_ids.size()),
      .skipped_existing_count = 0,
      .updated_existing_count = preview_result->recorded_count,
  };
}

auto set_infinity_nikki_world_record(Core::State::AppState& app_state,
                                     const Types::SetInfinityNikkiWorldRecordParams& params)
    -> std::expected<Types::OperationResult, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before updating Infinity Nikki world record: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto normalized_world_id =
      params.world_id.has_value()
          ? std::optional<std::string>{normalize_world_id_copy(params.world_id.value())}
          : std::nullopt;
  if (normalized_world_id.has_value() && normalized_world_id->empty()) {
    normalized_world_id = std::nullopt;
  }

  std::expected<void, std::string> write_result;
  if (normalized_world_id.has_value()) {
    write_result = upsert_infinity_nikki_user_record_key(app_state, params.asset_id, "world_id",
                                                         normalized_world_id.value());
  } else {
    write_result = delete_infinity_nikki_user_record_keys(app_state, params.asset_id, {"world_id"});
  }

  if (!write_result) {
    return std::unexpected(write_result.error());
  }

  return Types::OperationResult{.success = true,
                                .message = "Infinity Nikki world record updated successfully",
                                .affected_count = 1};
}

auto get_infinity_nikki_metadata_names(Core::State::AppState& app_state,
                                       const Types::GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<Types::InfinityNikkiMetadataNames, std::string>> {
  co_return co_await InfinityNikkiMetadataDict::resolve_metadata_names(app_state, params);
}

// ============= 维护服务实现 =============

auto load_asset_cache(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Types::Metadata>, std::string> {
  std::string sql = R"(
    SELECT id, name, path, type,
           NULL AS dominant_color_hex,
           rating, review_flag,
           description, width, height, size, extension, mime_type, hash,
           NULL AS root_id, NULL AS relative_path, folder_id,
           file_created_at, file_modified_at,
           created_at, updated_at
    FROM assets
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
                             .path = asset.path,
                             .size = asset.size.value_or(0),
                             .file_modified_at = asset.file_modified_at.value_or(0),
                             .hash = asset.hash.value_or("")};

    cache.emplace(asset.path, std::move(metadata));
  }
  Logger().info("Loaded {} assets into memory cache", cache.size());
  return cache;
}

}  // namespace Features::Gallery::Asset::Service
