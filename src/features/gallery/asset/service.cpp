module;

module Features.Gallery.Asset.Service;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.OriginalLocator;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.QuerySupport;
import Features.Gallery.Color.Repository;
import Utils.Logger;

namespace Features::Gallery::Asset::Service {

// ============= 内部辅助函数 =============

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

// ============= 查询服务实现 =============

// 统一的资产查询函数
auto query_assets(Core::State::AppState& app_state, const Types::QueryAssetsParams& params)
    -> std::expected<Types::ListResponse, std::string> {
  // 1. 构建通用WHERE条件
  auto where_result = QuerySupport::build_unified_where_clause(params.filters);
  if (!where_result) {
    return std::unexpected(where_result.error());
  }
  auto [where_clause, where_params] = std::move(where_result.value());

  // 2. 验证和构建 ORDER BY
  auto order_config = QuerySupport::build_query_order_config(params.sort_by, params.sort_order);

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
    auto active_index_result = QuerySupport::find_active_asset_index(
        app_state, params.filters, order_config, params.active_asset_id.value());
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
  auto where_result = QuerySupport::build_unified_where_clause(params.filters);
  if (!where_result) {
    return std::unexpected(where_result.error());
  }
  auto [where_clause, where_params] = std::move(where_result.value());

  auto order_config = QuerySupport::build_query_order_config(params.sort_by, params.sort_order);

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
  filters.color_hexes = params.color_hexes;
  filters.color_match_mode = params.color_match_mode;
  filters.color_distance = params.color_distance;

  auto order_config = QuerySupport::build_query_order_config(
      std::optional<std::string>{"created_at"}, params.sort_order);

  // 复用统一的 WHERE 条件构建器
  auto where_result = QuerySupport::build_unified_where_clause(filters);
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
    auto active_index_result = QuerySupport::find_active_asset_index(
        app_state, filters, order_config, params.active_asset_id.value());
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
  if (!QuerySupport::validate_month_format(params.month)) {
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
