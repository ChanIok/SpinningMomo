module;

export module Core.State;

import std;
// import Core.Async.State; // 被移除
// import Core.Events;
// import Core.I18n.State;
import Core.RpcHandlers.State; // Core::RpcHandlers的模板依赖
// import Core.WebView.State; // 改为前向声明
import Features.Letterbox.State;
import Features.Notifications.State;
import Features.Overlay.State;
import Features.Preview.State;
import Features.Screenshot.State;
// import Features.Settings.State; // 改为前向声明
import Types.UI;
import UI.AppWindow.State;
import UI.TrayIcon.State;
import UI.TrayMenu.State;
import Vendor.Windows;

export namespace Core::Async::State {
struct AsyncRuntimeState;
}

export namespace Core::Events {
struct EventBus;
}

export namespace Core::I18n::State {
struct I18nState;
}

export namespace Core::WebView::State {
struct WebViewState;
}

export namespace Features::Settings::State {
struct SettingsState;
}

// export namespace Core::RpcHandlers::State {
// struct RpcHandlerState;
// }

export namespace Core::State {

export struct AppState {
  // 应用级状态
  std::unique_ptr<Core::Async::State::AsyncRuntimeState> async_runtime;
  Types::UI::D2DRenderState d2d_render;
  std::unique_ptr<Core::Events::EventBus> event_bus;
  std::unique_ptr<Core::I18n::State::I18nState> i18n;
  std::unique_ptr<Core::RpcHandlers::State::RpcHandlerState> rpc_handlers;
  std::unique_ptr<Core::WebView::State::WebViewState> webview;

  // 应用设置状态（包含配置和计算状态）
  std::unique_ptr<Features::Settings::State::SettingsState> settings;

  // UI状态
  UI::AppWindow::State::Data app_window;
  UI::TrayIcon::State::Data tray_icon;
  UI::TrayMenu::State::Data tray_menu;

  // 功能模块状态
  Features::Letterbox::State::LetterboxState letterbox;
  Features::Notifications::State::NotificationSystemState notifications;
  Features::Overlay::State::OverlayState overlay;
  Features::Preview::State::PreviewState preview;
  Features::Screenshot::State::ScreenshotState screenshot;
};

// 根据DPI更新渲染状态
export auto update_render_dpi(AppState& state, Vendor::Windows::UINT new_dpi) -> void {
  state.app_window.window.dpi = new_dpi;
  state.d2d_render.needs_font_update = true;
  state.app_window.layout.update_dpi_scaling(new_dpi);
  state.tray_menu.layout.update_dpi_scaling(new_dpi);
}

}  // namespace Core::State