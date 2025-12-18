module;

module Core.Events.Handlers.Feature;

import std;
import Core.Events;
import Core.State;
import UI.AppWindow;
import UI.AppWindow.Events;
import Features.Screenshot.UseCase;
import Features.WindowControl.UseCase;

namespace Core::Events::Handlers {

// 注册功能相关的事件处理器
// 注：大部分功能已迁移至功能注册表（Features.Registry）
// 此处仅保留通过热键/系统事件触发的处理器
auto register_feature_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // === 截图功能 ===
  // 通过热键触发的截图事件
  subscribe<UI::AppWindow::Events::CaptureEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::CaptureEvent& event) {
        Features::Screenshot::UseCase::handle_capture_event(app_state, event);
      });

  // === 窗口控制功能 ===
  // 通过UI菜单选择触发的窗口调整事件
  subscribe<UI::AppWindow::Events::RatioChangeEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::RatioChangeEvent& event) {
        Features::WindowControl::UseCase::handle_ratio_changed(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::ResolutionChangeEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::ResolutionChangeEvent& event) {
        Features::WindowControl::UseCase::handle_resolution_changed(app_state, event);
        UI::AppWindow::request_repaint(app_state);
      });

  subscribe<UI::AppWindow::Events::WindowSelectionEvent>(
      *app_state.events, [&app_state](const UI::AppWindow::Events::WindowSelectionEvent& event) {
        Features::WindowControl::UseCase::handle_window_selected(app_state, event);
      });
}

}  // namespace Core::Events::Handlers