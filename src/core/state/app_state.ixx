module;

export module Core.State;

import std;

namespace Core::RPC::State {
export struct RpcState;
}

namespace Core::Async::State {
export struct AsyncState;
}

namespace Core::Events::State {
export struct EventsState;
}

namespace Core::I18n::State {
export struct I18nState;
}

namespace Core::WebView::State {
export struct WebViewState;
}

namespace Core::State::AppInfo {
export struct AppInfoState;
}

namespace Features::Settings::State {
export struct SettingsState;
}

namespace Features::Update::State {
export struct UpdateState;
}

namespace UI::AppWindow::State {
export struct AppWindowState;
}

namespace UI::TrayIcon::State {
export struct TrayIconState;
}

namespace UI::ContextMenu::State {
export struct ContextMenuState;
}

namespace Features::Letterbox::State {
export struct LetterboxState;
}

namespace Features::Notifications::State {
export struct NotificationSystemState;
}

namespace Features::Gallery::State {
export struct GalleryState;
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

namespace Features::Recording::State {
export struct RecordingState;
}

namespace Core::HttpServer::State {
export struct HttpServerState;
}

namespace Core::Database::State {
export struct DatabaseState;
}

namespace Core::WorkerPool::State {
export struct WorkerPoolState;
}

namespace Core::Commands::State {
export struct CommandState;
}

namespace Core::State {

export struct AppState {
  AppState();
  ~AppState();

  // 应用级状态
  std::unique_ptr<Core::RPC::State::RpcState> rpc;
  std::unique_ptr<Core::Async::State::AsyncState> async;
  std::unique_ptr<Core::Events::State::EventsState> events;
  std::unique_ptr<Core::I18n::State::I18nState> i18n;
  std::unique_ptr<Core::WebView::State::WebViewState> webview;
  std::unique_ptr<Core::State::AppInfo::AppInfoState> app_info;
  std::unique_ptr<Core::Database::State::DatabaseState> database;
  std::unique_ptr<Core::HttpServer::State::HttpServerState> http_server;
  std::unique_ptr<Core::WorkerPool::State::WorkerPoolState> worker_pool;
  std::unique_ptr<Core::Commands::State::CommandState> commands;

  // 应用设置状态（包含配置和计算状态）
  std::unique_ptr<Features::Settings::State::SettingsState> settings;

  // 更新模块状态
  std::unique_ptr<Features::Update::State::UpdateState> updater;

  // UI状态
  std::unique_ptr<UI::AppWindow::State::AppWindowState> app_window;
  std::unique_ptr<UI::TrayIcon::State::TrayIconState> tray_icon;
  std::unique_ptr<UI::ContextMenu::State::ContextMenuState> context_menu;

  // 功能模块状态
  std::unique_ptr<Features::Letterbox::State::LetterboxState> letterbox;
  std::unique_ptr<Features::Notifications::State::NotificationSystemState> notifications;
  std::unique_ptr<Features::Gallery::State::GalleryState> gallery;
  std::unique_ptr<Features::Overlay::State::OverlayState> overlay;
  std::unique_ptr<Features::Preview::State::PreviewState> preview;
  std::unique_ptr<Features::Screenshot::State::ScreenshotState> screenshot;
  std::unique_ptr<Features::Recording::State::RecordingState> recording;
};

}  // namespace Core::State