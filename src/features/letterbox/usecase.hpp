#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "ui/floating_window/events.hpp"

namespace features::letterbox {

// 切换黑边模式
auto toggle_letterbox(core::AppState& state) -> void;

}  // namespace features::letterbox
