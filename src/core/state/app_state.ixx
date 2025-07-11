module;

export module Core.State;

import std;
import Core.Async.State;
import Core.Config.State;
import Core.Events;
import Core.RpcHandlers.State;
import Core.WebView.State;
import Features.Letterbox.State;
import Features.Notifications.State;
import Features.Overlay.State;
import Features.Preview.State;
import Features.Screenshot.State;
import Types.UI;
import UI.AppWindow.State;
import UI.TrayIcon.State;
import UI.TrayMenu.State;
import Vendor.Windows;

export namespace Core::State {

export struct AppState {
  // 应用级状态
  Core::Async::State::AsyncRuntimeState async_runtime;
  Core::Config::State::AppConfig config;
  Types::UI::D2DRenderState d2d_render;
  Core::Events::EventBus event_bus;
  Core::RpcHandlers::State::RpcHandlerState rpc_handlers;
  Core::WebView::State::WebViewState webview;

  // 功能模块状态
  UI::AppWindow::State app_window;
  Features::Letterbox::State::LetterboxState letterbox;
  Features::Notifications::State::NotificationSystemState notifications;
  Features::Overlay::State::OverlayState overlay;
  Features::Preview::State::PreviewState preview;
  Features::Screenshot::State::ScreenshotState screenshot;
  UI::TrayIcon::State::Data tray_icon;
  UI::TrayMenu::State::Data tray_menu;
};

// 根据DPI更新渲染状态
export auto update_render_dpi(AppState& state, Vendor::Windows::UINT new_dpi) -> void {
  state.app_window.window.dpi = new_dpi;
  state.d2d_render.needs_font_update = true;
  state.app_window.layout.update_dpi_scaling(new_dpi);
  state.tray_menu.layout.update_dpi_scaling(new_dpi);
}

}  // namespace Core::State