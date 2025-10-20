module;

export module Features.Gallery.Tag.Repository;

import std;
import Core.State;
import Features.Gallery.Types;

export namespace Features::Gallery::Tag::Repository {

// ============= 基本 CRUD 操作 =============

// 创建标签
auto create_tag(Core::State::AppState& app_state, const Types::CreateTagParams& params)
    -> std::expected<std::int64_t, std::string>;

// 根据 ID 获取标签
auto get_tag_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Tag>, std::string>;

// 根据名称获取标签（可选指定父级）
auto get_tag_by_name(Core::State::AppState& app_state, const std::string& name,
                     std::optional<std::int64_t> parent_id = std::nullopt)
    -> std::expected<std::optional<Types::Tag>, std::string>;

// 更新标签
auto update_tag(Core::State::AppState& app_state, const Types::UpdateTagParams& params)
    -> std::expected<void, std::string>;

// 删除标签（级联删除子标签和关联）
auto delete_tag(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// 获取所有标签（扁平列表）
auto list_all_tags(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Tag>, std::string>;

// ============= 资产-标签关联操作 =============

// 为资产添加标签
auto add_tags_to_asset(Core::State::AppState& app_state,
                       const Types::AddTagsToAssetParams& params)
    -> std::expected<void, std::string>;

// 从资产移除标签
auto remove_tags_from_asset(Core::State::AppState& app_state,
                            const Types::RemoveTagsFromAssetParams& params)
    -> std::expected<void, std::string>;

// 替换资产的所有标签
auto replace_asset_tags(Core::State::AppState& app_state, std::int64_t asset_id,
                        const std::vector<std::int64_t>& tag_ids)
    -> std::expected<void, std::string>;

// 获取资产的所有标签
auto get_asset_tags(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Types::Tag>, std::string>;

// 批量获取多个资产的标签（返回 asset_id -> tags 映射）
auto get_tags_by_asset_ids(Core::State::AppState& app_state,
                            const std::vector<std::int64_t>& asset_ids)
    -> std::expected<std::unordered_map<std::int64_t, std::vector<Types::Tag>>,
                     std::string>;

// ============= 统计功能 =============

// 获取所有标签的使用统计
auto get_tag_stats(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::TagStats>, std::string>;

// ============= 标签树构建 =============

// 获取标签树结构
auto get_tag_tree(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::TagTreeNode>, std::string>;

}  // namespace Features::Gallery::Tag::Repository
