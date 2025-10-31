module;

export module Features.Gallery.Asset.Repository;

import std;
import Core.State;
import Core.Database.Types;
import Features.Gallery.Types;

namespace Features::Gallery::Asset::Repository {

// 基本操作
export auto create_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<std::int64_t, std::string>;

export auto get_asset_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Asset>, std::string>;

export auto get_asset_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Asset>, std::string>;

export auto update_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<void, std::string>;

export auto delete_asset(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

// 批量操作
export auto batch_create_asset(Core::State::AppState& app_state,
                               const std::vector<Types::Asset>& items)
    -> std::expected<std::vector<std::int64_t>, std::string>;

export auto batch_update_asset(Core::State::AppState& app_state,
                               const std::vector<Types::Asset>& items)
    -> std::expected<void, std::string>;

}  // namespace Features::Gallery::Asset::Repository
