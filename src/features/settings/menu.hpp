#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/settings/menu_types.hpp"

namespace features::settings::menu {

// === Getters Interface ===

// 获取当前的比例预设数据
auto get_ratios(const core::AppState& app_state) -> const std::vector<RatioPreset>&;

// 获取当前的分辨率预设数据
auto get_resolutions(const core::AppState& app_state) -> const std::vector<ResolutionPreset>&;

}  // namespace features::settings::menu
