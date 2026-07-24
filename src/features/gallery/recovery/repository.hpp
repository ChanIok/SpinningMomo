#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/recovery/types.hpp"

namespace Features::Gallery::Recovery::Repository {

auto get_state_by_root_path(Core::State::AppState& app_state, const std::string& root_path)
    -> std::expected<std::optional<Types::WatchRootRecoveryState>, std::string>;

auto upsert_state(Core::State::AppState& app_state, const Types::WatchRootRecoveryState& state)
    -> std::expected<void, std::string>;

auto delete_state_by_root_path(Core::State::AppState& app_state, const std::string& root_path)
    -> std::expected<void, std::string>;

}  // namespace Features::Gallery::Recovery::Repository
