module;

module Core.State;

import Core.Async.State;
import Core.Commands;
import Core.Commands.State;
import Core.Database.State;
import Core.Events.State;
import Core.HttpServer.State;
import Core.I18n.State;
import Core.RPC.State;
import Core.State.AppInfo;
import Core.WebView.State;
import Core.WorkerPool.State;
import Features.Letterbox.State;
import Features.Notifications.State;
import Features.Gallery.State;
import Features.Overlay.State;
import Features.Preview.State;
import Features.Screenshot.State;
import Features.Recording.State;
import Features.ReplayBuffer.State;
import Features.Settings.State;
import Features.Update.State;
import UI.FloatingWindow.State;
import UI.ContextMenu.State;
import UI.TrayIcon.State;

namespace Core::State {

AppState::AppState()
    : rpc(std::make_unique<Core::RPC::State::RpcState>()),
      async(std::make_unique<Core::Async::State::AsyncState>()),
      events(std::make_unique<Core::Events::State::EventsState>()),
      i18n(std::make_unique<Core::I18n::State::I18nState>()),
      webview(std::make_unique<Core::WebView::State::WebViewState>()),
      app_info(std::make_unique<Core::State::AppInfo::AppInfoState>()),
      database(std::make_unique<Core::Database::State::DatabaseState>()),
      http_server(std::make_unique<Core::HttpServer::State::HttpServerState>()),
      worker_pool(std::make_unique<Core::WorkerPool::State::WorkerPoolState>()),
      settings(std::make_unique<Features::Settings::State::SettingsState>()),
      update(std::make_unique<Features::Update::State::UpdateState>()),
      floating_window(std::make_unique<UI::FloatingWindow::State::FloatingWindowState>()),
      tray_icon(std::make_unique<UI::TrayIcon::State::TrayIconState>()),
      context_menu(std::make_unique<UI::ContextMenu::State::ContextMenuState>()),
      letterbox(std::make_unique<Features::Letterbox::State::LetterboxState>()),
      notifications(std::make_unique<Features::Notifications::State::NotificationSystemState>()),
      gallery(std::make_unique<Features::Gallery::State::GalleryState>()),
      overlay(std::make_unique<Features::Overlay::State::OverlayState>()),
      preview(std::make_unique<Features::Preview::State::PreviewState>()),
      screenshot(std::make_unique<Features::Screenshot::State::ScreenshotState>()),
      recording(std::make_unique<Features::Recording::State::RecordingState>()),
      replay_buffer(std::make_unique<Features::ReplayBuffer::State::ReplayBufferState>()),
      commands(std::make_unique<Core::Commands::State::CommandState>()) {}

AppState::~AppState() = default;

}  // namespace Core::State
