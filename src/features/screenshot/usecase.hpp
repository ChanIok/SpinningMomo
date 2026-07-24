#pragma once

#include "core/state/app_state.hpp"
#include "ui/floating_window/events.hpp"

namespace Features::Screenshot::UseCase {

// 截图（推荐使用）
auto capture(Core::State::AppState& state) -> void;

// 处理截图事件（Event版本，用于热键系统）
auto handle_capture_event(Core::State::AppState& state,
                          const UI::FloatingWindow::Events::CaptureEvent& event) -> void;

}  // namespace Features::Screenshot::UseCase
