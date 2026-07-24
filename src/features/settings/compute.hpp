#pragma once

#include "core/state/app_state.hpp"

namespace Features::Settings::Compute {

// 更新状态的计算部分
// 触发计算状态更新 (Reactivity Trigger)
auto trigger_compute(Core::State::AppState& app_state) -> bool;

}  // namespace Features::Settings::Compute