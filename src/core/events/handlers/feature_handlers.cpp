#include "core/events/handlers/feature_handlers.hpp"

#include "vendor/std.hpp"

#include "core/events/events.hpp"
#include "core/notifications/events.hpp"
#include "core/notifications/notifications.hpp"
#include "core/state/app_state.hpp"
#include "features/screenshot/usecase.hpp"
#include "features/window_control/usecase.hpp"
#include "ui/floating_window/events.hpp"
#include "ui/floating_window/floating_window.hpp"

namespace core::events::handlers {

// 注册功能相关的事件处理器
// 注：大部分功能已迁移至命令系统（core::commands）。
// 此处仅保留通过热键/系统事件触发的处理器
auto register_feature_handlers(core::AppState& app_state) -> void {
  using namespace core::events;

  // === 截图功能 ===
  // 通过热键触发的截图事件
  subscribe<ui::floating_window::events::CaptureEvent>(
      app_state, [&app_state](const ui::floating_window::events::CaptureEvent& event) {
        features::screenshot::handle_capture_event(app_state, event);
      });

  // === 窗口控制功能 ===
  // 通过UI菜单选择触发的窗口调整事件
  // 注：handle_xxx 会启动协程在 UI 线程执行，协程内部会在完成后请求重绘
  subscribe<ui::floating_window::events::RatioChangeEvent>(
      app_state, [&app_state](const ui::floating_window::events::RatioChangeEvent& event) {
        features::window_control::handle_ratio_changed(app_state, event);
      });

  subscribe<ui::floating_window::events::ResolutionChangeEvent>(
      app_state, [&app_state](const ui::floating_window::events::ResolutionChangeEvent& event) {
        features::window_control::handle_resolution_changed(app_state, event);
      });

  subscribe<ui::floating_window::events::WindowSelectionEvent>(
      app_state, [&app_state](const ui::floating_window::events::WindowSelectionEvent& event) {
        features::window_control::handle_window_selected(app_state, event);
      });

  // 录制状态由后台线程切换完成后，触发悬浮窗重绘以更新 toggle 显示
  subscribe<ui::floating_window::events::RecordingToggleEvent>(
      app_state, [&app_state](const ui::floating_window::events::RecordingToggleEvent&) {
        ui::floating_window::request_repaint(app_state);
      });

  // === 通知功能 ===
  // 非 UI 线程或有 action 的通知经 post_notification_request 投递，在此统一转到 UI 线程显示。
  subscribe<core::notifications::events::NotificationRequestEvent>(
      app_state, [&app_state](const core::notifications::events::NotificationRequestEvent& event) {
        core::notifications::show_notification(app_state, event.options);
      });
}

}  // namespace core::events::handlers
