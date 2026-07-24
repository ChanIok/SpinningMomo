#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::tag::repository {

auto create_tag(core::AppState& app_state, const CreateTagParams& params)
    -> std::expected<std::int64_t, std::string>;

auto get_tag_by_id(core::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Tag>, std::string>;

auto get_tag_by_name(core::AppState& app_state, const std::string& name,
                     std::optional<std::int64_t> parent_id = std::nullopt)
    -> std::expected<std::optional<Tag>, std::string>;

auto update_tag(core::AppState& app_state, const UpdateTagParams& params)
    -> std::expected<void, std::string>;

auto delete_tag(core::AppState& app_state, std::int64_t id) -> std::expected<void, std::string>;

auto add_tags_to_asset(core::AppState& app_state, const AddTagsToAssetParams& params)
    -> std::expected<void, std::string>;

auto add_tag_to_assets(core::AppState& app_state, const AddTagToAssetsParams& params)
    -> std::expected<OperationResult, std::string>;

auto remove_tag_from_assets(core::AppState& app_state, const RemoveTagFromAssetsParams& params)
    -> std::expected<OperationResult, std::string>;

auto remove_tags_from_asset(core::AppState& app_state, const RemoveTagsFromAssetParams& params)
    -> std::expected<void, std::string>;

auto get_asset_tags(core::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Tag>, std::string>;

auto get_tags_by_asset_ids(core::AppState& app_state, const std::vector<std::int64_t>& asset_ids)
    -> std::expected<std::unordered_map<std::int64_t, std::vector<Tag>>, std::string>;

auto get_tag_stats(core::AppState& app_state) -> std::expected<std::vector<TagStats>, std::string>;

auto get_tag_tree(core::AppState& app_state)
    -> std::expected<std::vector<TagTreeNode>, std::string>;

}  // namespace features::gallery::tag::repository
