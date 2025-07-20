module;

export module Core.State;

import std;

namespace Core::Async::State {
export struct AsyncRuntimeState;
}

namespace Core::Events {
export struct EventBus;
}

namespace Core::I18n::State {
export struct I18nState;
}

namespace Core::WebView::State {
export struct WebViewState;
}

namespace Features::Settings::State {
export struct SettingsState;
}

namespace UI::AppWindow::State {
export struct AppWindowState;
}

namespace UI::TrayIcon::State {
export struct TrayIconState;
}

namespace UI::TrayMenu::State {
export struct TrayMenuState;
}

namespace Features::Letterbox::State {
export struct LetterboxState;
}

namespace Features::Notifications::State {
export struct NotificationSystemState;
}

namespace Features::Overlay::State {
export struct OverlayState;
}

namespace Features::Preview::State {
export struct PreviewState;
}

namespace Features::Screenshot::State {
export struct ScreenshotState;
}

namespace Core::RpcHandlers::State {
export struct RpcHandlerState;
}

namespace Core::State {

export struct AppState {
  // 应用级状态
  std::unique_ptr<Core::Async::State::AsyncRuntimeState> async_runtime;
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
  std::unique_ptr<Features::Letterbox::State::LetterboxState> letterbox;
  std::unique_ptr<Features::Notifications::State::NotificationSystemState> notifications;
  std::unique_ptr<Features::Overlay::State::OverlayState> overlay;
  std::unique_ptr<Features::Preview::State::PreviewState> preview;
  std::unique_ptr<Features::Screenshot::State::ScreenshotState> screenshot;
};

}  // namespace Core::State