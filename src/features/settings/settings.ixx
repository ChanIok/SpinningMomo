module;

export module Features.Settings;

import std;
import Core.State;
import Features.Settings.Types;

namespace Features::Settings {

export auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

export auto get_settings(const Types::GetSettingsParams& params)
    -> std::expected<Types::GetSettingsResult, std::string>;

export auto update_settings(Core::State::AppState& app_state, const Types::UpdateSettingsParams& params)
    -> std::expected<Types::UpdateSettingsResult, std::string>;

}  // namespace Features::Settings