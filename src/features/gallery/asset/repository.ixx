module;

export module Features.Gallery.Asset.Repository;

import std;
import Core.State;
import Core.Database.Types;
import Features.Gallery.Types;

export namespace Features::Gallery::Asset::Repository {

// ============= 基本 CRUD 操作 =============

// 创建新资产项
auto create_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<std::int64_t, std::string>;

// 根据 ID 获取资产项
auto get_asset_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Asset>, std::string>;

// 根据文件路径获取资产项
auto get_asset_by_filepath(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Asset>, std::string>;

// 更新资产项
auto update_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<void, std::string>;

// 软删除资产项（设置 deleted_at）
auto soft_delete_asset(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// 硬删除资产项
auto hard_delete_asset(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// ============= 查询操作 =============

// 分页获取资产项列表
auto list_asset(Core::State::AppState& app_state, const Types::ListParams& params)
    -> std::expected<Types::ListResponse, std::string>;

// 获取资产项总数
auto count_asset(Core::State::AppState& app_state,
                 const std::optional<std::string>& filter_type = {},
                 const std::optional<std::string>& search_query = {})
    -> std::expected<int, std::string>;

// 获取资产统计信息
auto get_asset_stats(Core::State::AppState& app_state, const Types::GetStatsParams& params)
    -> std::expected<Types::Stats, std::string>;

// 获取资产项列表（可按文件夹筛选，可选包含子文件夹）
auto list_assets(Core::State::AppState& app_state, const Types::ListAssetsParams& params)
    -> std::expected<Types::ListResponse, std::string>;

// ============= 时间线视图查询 =============

// 获取时间线桶（月份统计）
auto get_timeline_buckets(Core::State::AppState& app_state,
                          const Types::TimelineBucketsParams& params)
    -> std::expected<Types::TimelineBucketsResponse, std::string>;

// 按月查询资产
auto get_assets_by_month(Core::State::AppState& app_state,
                         const Types::GetAssetsByMonthParams& params)
    -> std::expected<Types::GetAssetsByMonthResponse, std::string>;

// ============= 批量操作 =============

// 批量插入资产项
auto batch_create_asset(Core::State::AppState& app_state, const std::vector<Types::Asset>& items)
    -> std::expected<std::vector<std::int64_t>, std::string>;

// 批量更新资产项
auto batch_update_asset(Core::State::AppState& app_state, const std::vector<Types::Asset>& items)
    -> std::expected<void, std::string>;

// ============= 辅助函数 =============

// 清理软删除的记录（删除超过指定天数的软删除记录）
auto cleanup_soft_deleted_assets(Core::State::AppState& app_state, int days_old = 30)
    -> std::expected<int, std::string>;

// 加载数据库中的资产到内存缓存
auto load_asset_cache(Core::State::AppState& app_state)
    -> std::expected<std::unordered_map<std::string, Types::Metadata>, std::string>;

// 构建 WHERE 子句和参数（内部辅助函数）
struct AssetQueryBuilder {
  std::string where_clause;
  std::vector<Core::Database::Types::DbParam> params;
};

auto build_asset_list_query_conditions(const Types::ListParams& params) -> AssetQueryBuilder;

}  // namespace Features::Gallery::Asset::Repository
