#pragma once

#include "core/state/app_state.hpp"
#include "ui/floating_window/events.hpp"

namespace Features::Letterbox::UseCase {

// 切换黑边模式
auto toggle_letterbox(Core::State::AppState& state) -> void;

}  // namespace Features::Letterbox::UseCase
