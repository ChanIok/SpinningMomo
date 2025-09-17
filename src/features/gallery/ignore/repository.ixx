module;

export module Features.Gallery.Ignore.Repository;

import std;
import Core.State;
import Features.Gallery.Types;

export namespace Features::Gallery::Ignore::Repository {

// ============= 基本 CRUD 操作 =============

// 创建新的忽略规则
auto create_ignore_rule(Core::State::AppState& app_state, const Types::IgnoreRule& rule)
    -> std::expected<std::int64_t, std::string>;

// 根据ID获取忽略规则
auto get_ignore_rule_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::IgnoreRule>, std::string>;

// 更新忽略规则
auto update_ignore_rule(Core::State::AppState& app_state, const Types::IgnoreRule& rule)
    -> std::expected<void, std::string>;

// 删除忽略规则
auto delete_ignore_rule(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// ============= 基于文件夹的查询操作 =============

// 根据folder_id获取所有忽略规则
auto get_rules_by_folder_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string>;

// 根据目录路径获取所有忽略规则（便利方法）
auto get_rules_by_directory_path(Core::State::AppState& app_state,
                                 const std::string& directory_path)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string>;

// 获取全局忽略规则（folder_id为NULL的规则）
auto get_global_rules(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string>;

// ============= 批量操作 =============

// 批量创建忽略规则（基于folder_id）
auto batch_create_ignore_rules(Core::State::AppState& app_state, std::int64_t folder_id,
                               const std::vector<Types::ScanIgnoreRule>& scan_rules)
    -> std::expected<std::vector<std::int64_t>, std::string>;

// 批量更新忽略规则
auto batch_update_ignore_rules(Core::State::AppState& app_state,
                               const std::vector<Types::IgnoreRule>& rules)
    -> std::expected<void, std::string>;

// 删除文件夹的所有忽略规则
auto delete_rules_by_folder_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<int, std::string>;

// ============= 规则管理和维护 =============

// 启用/禁用规则
auto toggle_rule_enabled(Core::State::AppState& app_state, std::int64_t id, bool enabled)
    -> std::expected<void, std::string>;

// 清理无效的规则（关联的文件夹已删除）
auto cleanup_orphaned_rules(Core::State::AppState& app_state) -> std::expected<int, std::string>;

// 统计规则数量
auto count_rules(Core::State::AppState& app_state,
                 std::optional<std::int64_t> folder_id = std::nullopt)
    -> std::expected<int, std::string>;

}  // namespace Features::Gallery::Ignore::Repository
