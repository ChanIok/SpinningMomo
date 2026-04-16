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
           ia.asset_index AS asset_index
    FROM indexed_assets ia
    INNER JOIN filtered_assets fa ON fa.id = ia.id
    INNER JOIN asset_infinity_nikki_params p ON p.asset_id = ia.id
    WHERE p.nikki_loc_x IS NOT NULL
      AND p.nikki_loc_y IS NOT NULL
      AND fa.asset_type IN ('photo', 'live_photo')
    ORDER BY ia.asset_index
  )",
                                where_clause, order_config.indexed_order_clause);

  auto result = Core::Database::query<Types::PhotoMapPoint>(*app_state.database, sql, query_params);
  if (!result) {
    return std::unexpected("Failed to query photo map points: " + result.error());
  }

  return result.value();
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

  std::string user_record_sql = R"(
    SELECT code_type,
           code_value
    FROM asset_infinity_nikki_user_record
    WHERE asset_id = ?
  )";

  auto user_record_result = Core::Database::query_single<Types::InfinityNikkiUserRecord>(
      *app_state.database, user_record_sql, {params.asset_id});
  if (!user_record_result) {
    return std::unexpected("Failed to query Infinity Nikki user record: " +
                           user_record_result.error());
  }

  return Types::InfinityNikkiDetails{.extracted = extracted_result.value(),
                                     .user_record = user_record_result.value()};
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

  std::expected<void, std::string> write_result;
  if (normalized_code_value.has_value()) {
    auto result =
        Core::Database::execute(*app_state.database,
                                R"(
          INSERT INTO asset_infinity_nikki_user_record (asset_id, code_type, code_value)
          VALUES (?, ?, ?)
          ON CONFLICT(asset_id) DO UPDATE SET
            code_type = excluded.code_type,
            code_value = excluded.code_value
        )",
                                {params.asset_id, params.code_type, normalized_code_value.value()});
    if (!result) {
      write_result =
          std::unexpected("Failed to upsert Infinity Nikki user record: " + result.error());
    } else {
      write_result = {};
    }
  } else {
    auto result = Core::Database::execute(
        *app_state.database, "DELETE FROM asset_infinity_nikki_user_record WHERE asset_id = ?",
        {params.asset_id});
    if (!result) {
      write_result =
          std::unexpected("Failed to delete Infinity Nikki user record: " + result.error());
    } else {
      write_result = {};
    }
  }

  if (!write_result) {
    return std::unexpected(write_result.error());
  }

  auto result = Core::Database::query_scalar<std::int64_t>(*app_state.database, "SELECT changes()");
  if (!result) {
    return std::unexpected("Failed to query updated Infinity Nikki user record count: " +
                           result.error());
  }

  return Types::OperationResult{.success = true,
                                .message = "Infinity Nikki user record updated successfully",
                                .affected_count = result->value_or(0)};
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
