#pragma once

namespace Core::RPC::State {
struct RpcState;
}

namespace Core::Async::State {
struct AsyncState;
}

namespace Core::DialogService::State {
struct DialogServiceState;
}

namespace Core::Events::State {
struct EventsState;
}

namespace Core::I18n::State {
struct I18nState;
}

namespace Core::WebView::State {
struct WebViewState;
}

namespace Core::State::RuntimeInfo {
struct RuntimeInfoState;
}

namespace Core::Database::State {
struct DatabaseState;
}

namespace Core::HttpServer::State {
struct HttpServerState;
}

namespace Core::HttpClient::State {
struct HttpClientState;
}

namespace Core::WorkerPool::State {
struct WorkerPoolState;
}

namespace Core::Commands::State {
struct CommandState;
}

namespace Core::Tasks::State {
struct TaskState;
}

namespace Features::Settings::State {
struct SettingsState;
}

namespace Features::Update::State {
struct UpdateState;
}

namespace UI::SharedRenderResources::State {
struct SharedRenderResourcesState;
}

namespace UI::FloatingWindow::State {
struct FloatingWindowState;
}

namespace UI::TrayIcon::State {
struct TrayIconState;
}

namespace UI::ContextMenu::State {
struct ContextMenuState;
}

namespace UI::NotificationWindow::State {
struct NotificationWindowState;
}

namespace UI::PhotographyPanel::State {
struct PhotographyPanelState;
}

namespace Features::Letterbox::State {
struct LetterboxState;
}

namespace Features::Gallery::State {
struct GalleryState;
}

namespace Features::Overlay::State {
struct OverlayState;
}

namespace Features::Preview::State {
struct PreviewState;
}

namespace Features::WindowControl::State {
struct WindowControlState;
}

namespace Features::Screenshot::State {
struct ScreenshotState;
}

namespace Features::Recording::State {
struct RecordingState;
}

namespace Features::Photography::State {
struct PhotographyState;
}

namespace Core::State {

struct AppState {
  AppState();
  ~AppState();

  // 应用级状态
  std::unique_ptr<Core::RPC::State::RpcState> rpc;
  std::unique_ptr<Core::Async::State::AsyncState> async;
  std::unique_ptr<Core::DialogService::State::DialogServiceState> dialog_service;
  std::unique_ptr<Core::Events::State::EventsState> events;
  std::unique_ptr<Core::I18n::State::I18nState> i18n;
  std::unique_ptr<Core::WebView::State::WebViewState> webview;
  std::unique_ptr<Core::State::RuntimeInfo::RuntimeInfoState> runtime_info;
  std::unique_ptr<Core::Database::State::DatabaseState> database;
  std::unique_ptr<Core::HttpServer::State::HttpServerState> http_server;
  std::unique_ptr<Core::HttpClient::State::HttpClientState> http_client;
  std::unique_ptr<Core::WorkerPool::State::WorkerPoolState> worker_pool;
  std::unique_ptr<Core::Commands::State::CommandState> commands;
  std::unique_ptr<Core::Tasks::State::TaskState> tasks;

  // 应用设置状态（包含配置和计算状态）
  std::unique_ptr<Features::Settings::State::SettingsState> settings;

  // 更新模块状态
  std::unique_ptr<Features::Update::State::UpdateState> update;

  // UI状态
  std::unique_ptr<UI::SharedRenderResources::State::SharedRenderResourcesState>
      shared_render_resources;
  std::unique_ptr<UI::FloatingWindow::State::FloatingWindowState> floating_window;
  std::unique_ptr<UI::TrayIcon::State::TrayIconState> tray_icon;
  std::unique_ptr<UI::ContextMenu::State::ContextMenuState> context_menu;
  std::unique_ptr<UI::NotificationWindow::State::NotificationWindowState> notification_window;
  std::unique_ptr<UI::PhotographyPanel::State::PhotographyPanelState> photography_panel;

  // 功能模块状态
  std::unique_ptr<Features::Letterbox::State::LetterboxState> letterbox;
  std::unique_ptr<Features::Gallery::State::GalleryState> gallery;
  std::unique_ptr<Features::Overlay::State::OverlayState> overlay;
  std::unique_ptr<Features::Preview::State::PreviewState> preview;
  std::unique_ptr<Features::WindowControl::State::WindowControlState> window_control;
  std::unique_ptr<Features::Screenshot::State::ScreenshotState> screenshot;
  std::unique_ptr<Features::Recording::State::RecordingState> recording;
  std::unique_ptr<Features::Photography::State::PhotographyState> photography;
};

}  // namespace Core::State
