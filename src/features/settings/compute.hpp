#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::settings::compute {

// 更新状态的计算部分
// 触发计算状态更新 (Reactivity Trigger)
auto trigger_compute(core::AppState& app_state) -> bool;

}  // namespace features::settings::compute
