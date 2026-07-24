#pragma once

#include "core/state/app_state.hpp"
#include "ui/floating_window/events.hpp"

namespace Features::Overlay::UseCase {

// 切换叠加层功能
auto toggle_overlay(Core::State::AppState& state) -> void;

}  // namespace Features::Overlay::UseCase
