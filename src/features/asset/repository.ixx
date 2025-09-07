module;

export module Features.Asset.Repository;

import std;
import Core.State;
import Core.Database.Types;
import Features.Asset.Types;

export namespace Features::Asset::Repository {

// ============= 基本 CRUD 操作 =============

// 创建新资产项
auto create_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<std::int64_t, std::string>;

// 根据 ID 获取资产项
auto get_asset_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Asset>, std::string>;

// 根据文件路径获取资产项
auto get_asset_by_filepath(Core::State::AppState& app_state, const std::string& filepath)
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
    -> std::expected<Types::AssetListResponse, std::string>;

// 获取资产项总数
auto count_asset(Core::State::AppState& app_state,
                       const std::optional<std::string>& filter_type = {},
                       const std::optional<std::string>& search_query = {})
    -> std::expected<int, std::string>;

// 获取资产统计信息
auto get_asset_stats(Core::State::AppState& app_state, const Types::GetStatsParams& params)
    -> std::expected<Types::AssetStats, std::string>;

// ============= 批量操作 =============

// 批量插入资产项
auto batch_create_asset(Core::State::AppState& app_state,
                              const std::vector<Types::Asset>& items)
    -> std::expected<std::vector<std::int64_t>, std::string>;

// 批量更新资产项
auto batch_update_asset(Core::State::AppState& app_state,
                              const std::vector<Types::Asset>& items)
    -> std::expected<void, std::string>;

// ============= 辅助函数 =============

// 检查文件路径是否已存在
auto filepath_exists(Core::State::AppState& app_state, const std::string& filepath)
    -> std::expected<bool, std::string>;

// 清理软删除的记录（删除超过指定天数的软删除记录）
auto cleanup_soft_deleted_assets(Core::State::AppState& app_state, int days_old = 30)
    -> std::expected<int, std::string>;

// 构建 WHERE 子句和参数（内部辅助函数）
struct AssetQueryBuilder {
  std::string where_clause;
  std::vector<Core::Database::Types::DbParam> params;
};

auto build_asset_list_query_conditions(const Types::ListParams& params) -> AssetQueryBuilder;

}  // namespace Features::Asset::Repository
