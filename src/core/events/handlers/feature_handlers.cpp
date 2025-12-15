module;

module Core.Events.Handlers.Feature;

import std;
import Core.Events;
import Core.State;
import UI.AppWindow;
import UI.AppWindow.Events;
import Features.Preview.UseCase;
import Features.Overlay.UseCase;
import Features.Letterbox.UseCase;
import Features.Screenshot.UseCase;
import Features.Recording.UseCase;
import Features.WindowControl.UseCase;

namespace Core::Events::Handlers {

// 注册功能开关事件处理器
auto register_feature_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  subscribe<UI::AppWindow::Events::PreviewToggleEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::PreviewToggleEvent& event) {
        Features::Preview::UseCase::handle_preview_toggle(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::OverlayToggleEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::OverlayToggleEvent& event) {
        Features::Overlay::UseCase::handle_overlay_toggle(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::LetterboxToggleEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::LetterboxToggleEvent& event) {
        Features::Letterbox::UseCase::handle_letterbox_toggle(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::RecordingToggleEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::RecordingToggleEvent& event) {
        // 调用 UseCase 处理录制切换
        if (auto result = Features::Recording::UseCase::toggle_recording(app_state); !result) {
          // 这里可以添加错误处理，例如发送通知或显示消息
        }
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::CaptureEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::CaptureEvent& event) {
        Features::Screenshot::UseCase::handle_capture_event(app_state, event);
      });

  subscribe<UI::AppWindow::Events::ScreenshotsEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::ScreenshotsEvent& event) {
        Features::Screenshot::UseCase::handle_screenshots_event(app_state, event);
      });

  subscribe<UI::AppWindow::Events::RatioChangeEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::RatioChangeEvent& event) {
        Features::WindowControl::UseCase::handle_ratio_changed(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::ResolutionChangeEvent>(
      *app_state.events,
      [&app_state](const UI::AppWindow::Events::ResolutionChangeEvent& event) {
        Features::WindowControl::UseCase::handle_resolution_changed(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::ResetEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::ResetEvent& event) {
        Features::WindowControl::UseCase::handle_reset_event(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::WindowSelectionEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::WindowSelectionEvent& event) {
        Features::WindowControl::UseCase::handle_window_selected(app_state, event);
      });
}

}  // namespace Core::Events::Handlers