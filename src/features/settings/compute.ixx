module;

export module Features.Settings.Compute;

import std;
import Features.Settings.Types;
import Features.Settings.State;
import Types.Presets;

export namespace Features::Settings::Compute {

// 比例解析函数
auto parse_ratio(const std::string& id) -> std::optional<double>;

// 分辨率解析函数
auto parse_resolution(const std::string& id) -> std::optional<std::pair<int, int>>;

// 从配置计算完整的预设状态
auto compute_presets_from_config(const Types::AppSettings& config) -> State::ComputedPresets;

// 更新状态的计算部分
auto update_computed_state(State::SettingsState& state) -> bool;

}  // namespace Features::Settings::Compute