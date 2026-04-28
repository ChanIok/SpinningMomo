module;

export module Features.Gallery.Asset.QuerySupport;

import std;
import Core.State;
import Core.Database.Types;
import Features.Gallery.Types;

namespace Features::Gallery::Asset::QuerySupport {

export struct QueryOrderConfig {
  std::string sort_by;
  std::string sort_order;
  std::string asset_order_clause;
  // 给 ROW_NUMBER() 用的排序子句；需要基于中间列名而不是 assets 原始表达式。
  std::string indexed_order_clause;
};

export auto validate_month_format(const std::string& month) -> bool;

export auto build_query_order_config(std::optional<std::string> sort_by_param,
                                     std::optional<std::string> sort_order_param)
    -> QueryOrderConfig;

export auto build_unified_where_clause(const Features::Gallery::Types::QueryAssetsFilters& filters,
                                       std::string_view asset_table_alias = "")
    -> std::expected<std::pair<std::string, std::vector<Core::Database::Types::DbParam>>,
                     std::string>;

export auto find_active_asset_index(Core::State::AppState& app_state,
                                    const Features::Gallery::Types::QueryAssetsFilters& filters,
                                    const QueryOrderConfig& order_config,
                                    std::int64_t active_asset_id)
    -> std::expected<std::optional<std::int64_t>, std::string>;

}  // namespace Features::Gallery::Asset::QuerySupport
