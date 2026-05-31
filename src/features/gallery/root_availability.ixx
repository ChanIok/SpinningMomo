module;

export module Features.Gallery.RootAvailability;

import std;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;

namespace Features::Gallery::RootAvailability {

export constexpr std::chrono::milliseconds kDefaultRemoteProbeTimeout{1200};

export auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

export auto get_for_root_id(Core::State::AppState& app_state, std::int64_t root_id)
    -> std::optional<Features::Gallery::State::RootAvailability>;

export auto get_for_path(Core::State::AppState& app_state, const std::filesystem::path& root_path)
    -> Features::Gallery::State::RootAvailability;

export auto is_remote_unreachable(Core::State::AppState& app_state, std::int64_t root_id) -> bool;

export auto is_remote_unreachable(Core::State::AppState& app_state,
                                  const std::filesystem::path& root_path) -> bool;

export auto availability_to_string(Features::Gallery::State::RootAvailability availability)
    -> std::string_view;

}  // namespace Features::Gallery::RootAvailability
