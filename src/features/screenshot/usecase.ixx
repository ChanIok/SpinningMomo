module;

export module Features.Screenshot.UseCase;

import Core.State;
import UI.AppWindow.Events;

namespace Features::Screenshot::UseCase {

// 处理截图事件
export auto handle_capture_event(Core::State::AppState& state,
                                const UI::AppWindow::Events::CaptureEvent& event) -> void;

// 处理打开截图文件夹事件
export auto handle_screenshots_event(Core::State::AppState& state,
                                    const UI::AppWindow::Events::ScreenshotsEvent& event) -> void;

}  // namespace Features::Screenshot::UseCase