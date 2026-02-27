module;

module Core.Events.Handlers.Feature;

import std;
import Core.Events;
import Core.State;
import UI.FloatingWindow;
import UI.FloatingWindow.Events;
import Features.Screenshot.UseCase;
import Features.WindowControl.UseCase;
import Features.Notifications;

namespace Core::Events::Handlers {

// 注册功能相关的事件处理器
// 注:大部分功能已迁移至命令系统(Core.Commands)
// 此处仅保留通过热键/系统事件触发的处理器
auto register_feature_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // === 截图功能 ===
  // 通过热键触发的截图事件
  subscribe<UI::FloatingWindow::Events::CaptureEvent>(
      *app_state.events, [&app_state](const UI::FloatingWindow::Events::CaptureEvent& event) {
        Features::Screenshot::UseCase::handle_capture_event(app_state, event);
      });

  // === 窗口控制功能 ===
  // 通过UI菜单选择触发的窗口调整事件
  // 注：handle_xxx 会启动协程在 UI 线程执行，协程内部会在完成后请求重绘
  subscribe<UI::FloatingWindow::Events::RatioChangeEvent>(
      *app_state.events, [&app_state](const UI::FloatingWindow::Events::RatioChangeEvent& event) {
        Features::WindowControl::UseCase::handle_ratio_changed(app_state, event);
      });

  subscribe<UI::FloatingWindow::Events::ResolutionChangeEvent>(
      *app_state.events,
      [&app_state](const UI::FloatingWindow::Events::ResolutionChangeEvent& event) {
        Features::WindowControl::UseCase::handle_resolution_changed(app_state, event);
      });

  subscribe<UI::FloatingWindow::Events::WindowSelectionEvent>(
      *app_state.events,
      [&app_state](const UI::FloatingWindow::Events::WindowSelectionEvent& event) {
        Features::WindowControl::UseCase::handle_window_selected(app_state, event);
      });

  // 录制状态由后台线程切换完成后，触发悬浮窗重绘以更新 toggle 显示
  subscribe<UI::FloatingWindow::Events::RecordingToggleEvent>(
      *app_state.events, [&app_state](const UI::FloatingWindow::Events::RecordingToggleEvent&) {
        UI::FloatingWindow::request_repaint(app_state);
      });

  // === 通知功能 ===
  // 跨线程安全的通知显示（由 WorkerPool 等工作线程 post 事件，UI 线程处理）
  subscribe<UI::FloatingWindow::Events::NotificationEvent>(
      *app_state.events, [&app_state](const UI::FloatingWindow::Events::NotificationEvent& event) {
        Features::Notifications::show_notification(app_state, event.title, event.message);
      });
}

}  // namespace Core::Events::Handlers
