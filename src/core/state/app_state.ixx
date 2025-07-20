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
// import UI.AppWindow.State; // 改为前向声明
// import UI.TrayIcon.State; // 改为前向声明
// import UI.TrayMenu.State; // 改为前向声明
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

export namespace UI::AppWindow::State {
struct AppWindowState;
}

export namespace UI::TrayIcon::State {
struct TrayIconState;
}

export namespace UI::TrayMenu::State {
struct TrayMenuState;
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
  std::unique_ptr<UI::AppWindow::State::AppWindowState> app_window;
  std::unique_ptr<UI::TrayIcon::State::TrayIconState> tray_icon;
  std::unique_ptr<UI::TrayMenu::State::TrayMenuState> tray_menu;

  // 功能模块状态
  Features::Letterbox::State::LetterboxState letterbox;
  Features::Notifications::State::NotificationSystemState notifications;
  Features::Overlay::State::OverlayState overlay;
  Features::Preview::State::PreviewState preview;
  Features::Screenshot::State::ScreenshotState screenshot;
};

}  // namespace Core::State