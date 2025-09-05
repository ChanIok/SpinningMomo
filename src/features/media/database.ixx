module;

export module Features.Media.Database;

import std;
import Core.State;
import Core.Database.Types;
import Features.Media.Types;

export namespace Features::Media::Database {

// ============= 基本 CRUD 操作 =============

// 创建新媒体项
auto create_media_item(Core::State::AppState& app_state, const Types::MediaItem& item)
    -> std::expected<std::int64_t, std::string>;

// 根据 ID 获取媒体项
auto get_media_item_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::MediaItem>, std::string>;

// 根据文件路径获取媒体项
auto get_media_item_by_filepath(Core::State::AppState& app_state, const std::string& filepath)
    -> std::expected<std::optional<Types::MediaItem>, std::string>;

// 更新媒体项
auto update_media_item(Core::State::AppState& app_state, const Types::MediaItem& item)
    -> std::expected<void, std::string>;

// 软删除媒体项（设置 deleted_at）
auto soft_delete_media_item(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// 硬删除媒体项
auto hard_delete_media_item(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// ============= 查询操作 =============

// 分页获取媒体项列表
auto list_media_items(Core::State::AppState& app_state, const Types::ListMediaParams& params)
    -> std::expected<Types::MediaListResponse, std::string>;

// 获取媒体项总数
auto count_media_items(Core::State::AppState& app_state,
                       const std::optional<std::string>& filter_type = {},
                       const std::optional<std::string>& search_query = {})
    -> std::expected<int, std::string>;

// 获取媒体统计信息
auto get_media_stats(Core::State::AppState& app_state, const Types::GetStatsParams& params)
    -> std::expected<Types::MediaStats, std::string>;

// ============= 批量操作 =============

// 批量插入媒体项
auto batch_create_media_items(Core::State::AppState& app_state,
                              const std::vector<Types::MediaItem>& items)
    -> std::expected<std::vector<std::int64_t>, std::string>;

// 批量更新媒体项
auto batch_update_media_items(Core::State::AppState& app_state,
                              const std::vector<Types::MediaItem>& items)
    -> std::expected<void, std::string>;

// ============= 辅助函数 =============

// 检查文件路径是否已存在
auto filepath_exists(Core::State::AppState& app_state, const std::string& filepath)
    -> std::expected<bool, std::string>;

// 清理软删除的记录（删除超过指定天数的软删除记录）
auto cleanup_soft_deleted(Core::State::AppState& app_state, int days_old = 30)
    -> std::expected<int, std::string>;

// 构建 WHERE 子句和参数（内部辅助函数）
struct QueryBuilder {
  std::string where_clause;
  std::vector<Core::Database::Types::DbParam> params;
};

auto build_list_query_conditions(const Types::ListMediaParams& params) -> QueryBuilder;

}  // namespace Features::Media::Database
