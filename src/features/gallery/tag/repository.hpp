#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::Tag::Repository {

auto create_tag(Core::State::AppState& app_state, const Types::CreateTagParams& params)
    -> std::expected<std::int64_t, std::string>;

auto get_tag_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Tag>, std::string>;

auto get_tag_by_name(Core::State::AppState& app_state, const std::string& name,
                     std::optional<std::int64_t> parent_id = std::nullopt)
    -> std::expected<std::optional<Types::Tag>, std::string>;

auto update_tag(Core::State::AppState& app_state, const Types::UpdateTagParams& params)
    -> std::expected<void, std::string>;

auto delete_tag(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

auto add_tags_to_asset(Core::State::AppState& app_state, const Types::AddTagsToAssetParams& params)
    -> std::expected<void, std::string>;

auto add_tag_to_assets(Core::State::AppState& app_state, const Types::AddTagToAssetsParams& params)
    -> std::expected<Types::OperationResult, std::string>;

auto remove_tag_from_assets(Core::State::AppState& app_state,
                            const Types::RemoveTagFromAssetsParams& params)
    -> std::expected<Types::OperationResult, std::string>;

auto remove_tags_from_asset(Core::State::AppState& app_state,
                            const Types::RemoveTagsFromAssetParams& params)
    -> std::expected<void, std::string>;

auto get_asset_tags(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Types::Tag>, std::string>;

auto get_tags_by_asset_ids(Core::State::AppState& app_state,
                           const std::vector<std::int64_t>& asset_ids)
    -> std::expected<std::unordered_map<std::int64_t, std::vector<Types::Tag>>, std::string>;

auto get_tag_stats(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::TagStats>, std::string>;

auto get_tag_tree(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::TagTreeNode>, std::string>;

}  // namespace Features::Gallery::Tag::Repository
