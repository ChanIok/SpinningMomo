module;

export module Core.State;

import std;
import Core.Events;
import Types.Config;
import Types.UI;
import Types.Notification;
import Types.TrayIcon;
import Types.Preview;
import Types.Screenshot;
import UI.AppWindow.State;
import Vendor.Windows;

export namespace Core::State {

// 应用程序状态（模块化组合）
export struct AppState {
  // 应用级状态
  Types::Config::AppConfig config;
  Core::Events::EventBus event_bus;
  Types::UI::D2DRenderState d2d_render;

  // 功能模块状态
  UI::AppWindow::State app_window;
  Types::Notification::NotificationSystemState notifications;
  Types::TrayIcon::Data tray_icon;
  Types::Preview::PreviewState preview;
  Types::Screenshot::ScreenshotState screenshot;

  // 便捷访问方法（保持向后兼容）
  auto is_window_valid() const -> bool { return app_window.is_window_valid(); }
  auto get_total_width() const -> int { return app_window.get_total_width(); }
  auto get_menu_item_count() const -> size_t { return app_window.get_menu_item_count(); }
};

// 根据DPI更新渲染状态
export auto update_render_dpi(AppState& state, Vendor::Windows::UINT new_dpi) -> void {
  state.app_window.window.dpi = new_dpi;
  state.app_window.layout.update_dpi_scaling(new_dpi);
}

}  // namespace Core::State