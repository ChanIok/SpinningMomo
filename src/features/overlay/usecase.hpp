#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "ui/floating_window/events.hpp"

namespace features::overlay {

// 切换叠加层功能
auto toggle_overlay(core::AppState& state) -> void;

}  // namespace features::overlay
