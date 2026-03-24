module;

export module Features.Gallery.Recovery.Repository;

import std;
import Core.State;
export import Features.Gallery.Recovery.Types;

namespace Features::Gallery::Recovery::Repository {

export auto get_state_by_root_path(Core::State::AppState& app_state, const std::string& root_path)
    -> std::expected<std::optional<Types::WatchRootRecoveryState>, std::string>;

export auto upsert_state(Core::State::AppState& app_state,
                         const Types::WatchRootRecoveryState& state)
    -> std::expected<void, std::string>;

export auto delete_state_by_root_path(Core::State::AppState& app_state,
                                      const std::string& root_path)
    -> std::expected<void, std::string>;

}  // namespace Features::Gallery::Recovery::Repository
