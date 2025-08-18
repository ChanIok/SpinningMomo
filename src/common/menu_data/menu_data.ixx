module;

export module Common.MenuData;

import std;
import Core.State;
import Common.MenuData.Types;

namespace Common::MenuData {

// 获取当前的比例预设数据
export auto get_current_aspect_ratios(const Core::State::AppState& state)
    -> const std::vector<Common::MenuData::Types::RatioPreset>&;

// 获取当前的分辨率预设数据
export auto get_current_resolutions(const Core::State::AppState& state)
    -> const std::vector<Common::MenuData::Types::ResolutionPreset>&;

// 获取当前的功能项数据
export auto get_current_feature_items(const Core::State::AppState& state)
    -> const std::vector<Common::MenuData::Types::ComputedFeatureItem>&;

}  // namespace Common::MenuData