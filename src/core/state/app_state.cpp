module;

module Core.State;

import Core.Async.State;
import Core.Database.State;
import Core.Events.State;
import Core.HttpServer.State;
import Core.I18n.State;
import Core.RPC.State;
import Core.State.AppInfo;
import Core.WebView.State;
import Features.Letterbox.State;
import Features.Notifications.State;
import Features.Overlay.State;
import Features.Preview.State;
import Features.Screenshot.State;
import Features.Settings.State;
import Features.Updater.State;
import UI.AppWindow.State;
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
      settings(std::make_unique<Features::Settings::State::SettingsState>()),
      updater(std::make_unique<Features::Updater::State::UpdateState>()),
      app_window(std::make_unique<UI::AppWindow::State::AppWindowState>()),
      tray_icon(std::make_unique<UI::TrayIcon::State::TrayIconState>()),
      context_menu(std::make_unique<UI::ContextMenu::State::ContextMenuState>()),
      letterbox(std::make_unique<Features::Letterbox::State::LetterboxState>()),
      notifications(std::make_unique<Features::Notifications::State::NotificationSystemState>()),
      overlay(std::make_unique<Features::Overlay::State::OverlayState>()),
      preview(std::make_unique<Features::Preview::State::PreviewState>()),
      screenshot(std::make_unique<Features::Screenshot::State::ScreenshotState>()) {}

AppState::~AppState() = default;

}  // namespace Core::State
