module;

export module Features.Screenshot.UseCase;

import Core.State;
import UI.FloatingWindow.Events;

namespace Features::Screenshot::UseCase {

// 截图（推荐使用）
export auto capture(Core::State::AppState& state) -> void;

// 处理截图事件（Event版本，用于热键系统）
export auto handle_capture_event(Core::State::AppState& state,
                                 const UI::FloatingWindow::Events::CaptureEvent& event) -> void;

}  // namespace Features::Screenshot::UseCase
