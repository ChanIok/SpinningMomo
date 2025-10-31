module;

export module Features.Gallery.Tag.Repository;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Tag::Repository {

export auto create_tag(Core::State::AppState& app_state, const Types::CreateTagParams& params)
    -> std::expected<std::int64_t, std::string>;

export auto get_tag_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Tag>, std::string>;

export auto get_tag_by_name(Core::State::AppState& app_state, const std::string& name,
                            std::optional<std::int64_t> parent_id = std::nullopt)
    -> std::expected<std::optional<Types::Tag>, std::string>;

export auto update_tag(Core::State::AppState& app_state, const Types::UpdateTagParams& params)
    -> std::expected<void, std::string>;

export auto delete_tag(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

export auto add_tags_to_asset(Core::State::AppState& app_state,
                              const Types::AddTagsToAssetParams& params)
    -> std::expected<void, std::string>;

export auto remove_tags_from_asset(Core::State::AppState& app_state,
                                   const Types::RemoveTagsFromAssetParams& params)
    -> std::expected<void, std::string>;

export auto get_asset_tags(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Types::Tag>, std::string>;

export auto get_tags_by_asset_ids(Core::State::AppState& app_state,
                                  const std::vector<std::int64_t>& asset_ids)
    -> std::expected<std::unordered_map<std::int64_t, std::vector<Types::Tag>>, std::string>;

export auto get_tag_stats(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::TagStats>, std::string>;

export auto get_tag_tree(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::TagTreeNode>, std::string>;

}  // namespace Features::Gallery::Tag::Repository
