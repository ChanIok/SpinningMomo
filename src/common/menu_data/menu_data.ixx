module;

export module Common.MenuData;

import std;
import Core.State;
import Types.Presets;

namespace Common::MenuData {

// 获取当前的比例预设数据
export auto get_current_aspect_ratios(const Core::State::AppState& state)
    -> const std::vector<Types::Presets::RatioPreset>&;

// 获取当前的分辨率预设数据
export auto get_current_resolutions(const Core::State::AppState& state)
    -> const std::vector<Types::Presets::ResolutionPreset>&;

}  // namespace Common::MenuData