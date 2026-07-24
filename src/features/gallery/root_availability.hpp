#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/state.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::RootAvailability {

constexpr std::chrono::milliseconds kDefaultRemoteProbeTimeout{1200};

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

auto get_for_root_id(Core::State::AppState& app_state, std::int64_t root_id)
    -> std::optional<Features::Gallery::State::RootAvailability>;

auto get_for_path(Core::State::AppState& app_state, const std::filesystem::path& root_path)
    -> Features::Gallery::State::RootAvailability;

auto is_remote_unreachable(Core::State::AppState& app_state, std::int64_t root_id) -> bool;

auto is_remote_unreachable(Core::State::AppState& app_state, const std::filesystem::path& root_path)
    -> bool;

auto availability_to_string(Features::Gallery::State::RootAvailability availability)
    -> std::string_view;

}  // namespace Features::Gallery::RootAvailability
