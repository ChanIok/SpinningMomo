module;

module Features.Gallery.Asset.Service;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.Types;
import Features.Gallery.State;
import Features.Gallery.Asset.Repository;
import Utils.Logger;
import Utils.LRUCache;

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

// 构建统一的WHERE条件
auto build_unified_where_clause(const Types::QueryAssetsFilters& filters)
    -> std::pair<std::string, std::vector<Core::Database::Types::DbParam>> {
  std::vector<std::string> conditions;
  std::vector<Core::Database::Types::DbParam> params;

  // 文件夹筛选
  if (filters.folder_id.has_value()) {
    if (filters.include_subfolders.value_or(false)) {
      // 使用递归CTE查询当前文件夹及所有子文件夹
      conditions.push_back(R"(folder_id IN (
        WITH RECURSIVE folder_hierarchy AS (
          SELECT id FROM folders WHERE id = ?
          UNION ALL
          SELECT f.id FROM folders f
          INNER JOIN folder_hierarchy fh ON f.parent_id = fh.id
        )
        SELECT id FROM folder_hierarchy
      ))");
      params.push_back(filters.folder_id.value());
    } else {
      // 只查询当前文件夹
      conditions.push_back("folder_id = ?");
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
          "strftime('%Y-%m', datetime(COALESCE(file_created_at, created_at)/1000, 'unixepoch')) = "
          "?");
      params.push_back(filters.month.value());
    }
  }

  // 年份筛选
  if (filters.year.has_value()) {
    conditions.push_back(
        "strftime('%Y', datetime(COALESCE(file_created_at, created_at)/1000, 'unixepoch')) = ?");
    params.push_back(filters.year.value());
  }

  // 类型筛选
  if (filters.type.has_value() && !filters.type->empty()) {
    conditions.push_back("type = ?");
    params.push_back(filters.type.value());
  }

  // 搜索
  if (filters.search.has_value() && !filters.search->empty()) {
    conditions.push_back("name LIKE ?");
    params.push_back("%" + filters.search.value() + "%");
  }

  // 标签筛选
  if (filters.tag_ids.has_value() && !filters.tag_ids->empty()) {
    std::string match_mode = filters.tag_match_mode.value_or("any");

    if (match_mode == "all") {
      // 匹配所有标签（AND）：资产必须拥有所有指定的标签
      for (const auto& tag_id : filters.tag_ids.value()) {
        conditions.push_back("id IN (SELECT asset_id FROM asset_tags WHERE tag_id = ?)");
        params.push_back(tag_id);
      }
    } else {
      // 匹配任一标签（OR）：资产拥有任意一个标签即可
      std::string placeholders = std::string(filters.tag_ids->size() * 2 - 1, '?');
      for (size_t i = 1; i < filters.tag_ids->size(); ++i) {
        placeholders[i * 2 - 1] = ',';
      }
      conditions.push_back(std::format(
          "id IN (SELECT asset_id FROM asset_tags WHERE tag_id IN ({}))", placeholders));
      for (const auto& tag_id : filters.tag_ids.value()) {
        params.push_back(tag_id);
      }
    }
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

  return {where_clause, params};
}

// ============= 查询服务实现 =============

// 统一的资产查询函数
auto query_assets(Core::State::AppState& app_state, const Types::QueryAssetsParams& params)
    -> std::expected<Types::ListResponse, std::string> {
  // 1. 构建通用WHERE条件
  auto [where_clause, where_params] = build_unified_where_clause(params.filters);

  // 2. 验证和构建 ORDER BY
  std::string sort_by = params.sort_by.value_or("created_at");
  std::string sort_order = params.sort_order.value_or("desc");

  // 安全的排序字段验证
  if (sort_by != "created_at" && sort_by != "name" && sort_by != "size" &&
      sort_by != "file_created_at") {
    sort_by = "created_at";
  }
  if (sort_order != "asc" && sort_order != "desc") {
    sort_order = "desc";
  }

  // 对于时间相关的排序，使用 COALESCE 处理
  std::string order_field = sort_by;
  if (sort_by == "created_at") {
    order_field = "COALESCE(file_created_at, created_at)";
  }

  std::string order_clause = std::format("ORDER BY {} {}", order_field, sort_order);

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
           description, width, height, size, extension, mime_type, hash, folder_id,
           file_created_at, file_modified_at,
           created_at, updated_at
    FROM assets
    {}
    {}
  )",
                                where_clause, order_clause);

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

  // 8. 预热缓存：将查询结果的 id->path 映射加入缓存
  if (app_state.gallery && !response.items.empty()) {
    std::vector<std::pair<std::int64_t, std::filesystem::path>> cache_items;
    cache_items.reserve(response.items.size());

    for (const auto& asset : response.items) {
      cache_items.emplace_back(asset.id, std::filesystem::path(asset.path));
    }

    Utils::LRUCache::warm_up(app_state.gallery->image_path_cache, cache_items);
    Logger().debug("Warmed up image path cache with {} items", cache_items.size());
  }

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
  filters.tag_ids = params.tag_ids;
  filters.tag_match_mode = params.tag_match_mode;

  // 复用统一的 WHERE 条件构建器
  auto [where_clause, query_params] = build_unified_where_clause(filters);

  // 构建查询
  std::string sql = std::format(R"(
    SELECT 
      strftime('%Y-%m', datetime(COALESCE(file_created_at, created_at)/1000, 'unixepoch')) as month,
      COUNT(*) as count
    FROM assets 
    {}
    GROUP BY month
    ORDER BY month DESC
  )",
                                where_clause);

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

// ============= 维护服务实现 =============

auto load_asset_cache(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Types::Metadata>, std::string> {
  std::string sql = R"(
    SELECT id, name, path, type,
           description, width, height, size, extension, mime_type, hash, folder_id,
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
