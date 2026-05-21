module;

export module Features.Settings.Menu;

import std;
import Core.State;
import Features.Settings.Menu.Types;

namespace Features::Settings::Menu {

// === Getters Interface ===

// 获取当前的比例预设数据
export auto get_ratios(const Core::State::AppState& app_state) -> const std::vector<RatioPreset>&;

// 获取当前的分辨率预设数据
export auto get_resolutions(const Core::State::AppState& app_state)
    -> const std::vector<ResolutionPreset>&;

}  // namespace Features::Settings::Menu
