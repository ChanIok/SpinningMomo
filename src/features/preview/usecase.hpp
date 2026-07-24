#pragma once

#include "core/state/app_state.hpp"
#include "ui/floating_window/events.hpp"

namespace Features::Preview::UseCase {

// 切换预览功能
auto toggle_preview(Core::State::AppState& state) -> void;

}  // namespace Features::Preview::UseCase
