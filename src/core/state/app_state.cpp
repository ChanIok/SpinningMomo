#include "core/state/app_state.hpp"

#include "core/async/state.hpp"
#include "core/commands/state.hpp"
#include "core/database/state.hpp"
#include "core/dialog_service/state.hpp"
#include "core/events/state.hpp"
#include "core/http_client/state.hpp"
#include "core/http_server/state.hpp"
#include "core/i18n/state.hpp"
#include "core/rpc/state.hpp"
#include "core/state/runtime_info.hpp"
#include "core/tasks/state.hpp"
#include "core/webview/state.hpp"
#include "core/worker_pool/state.hpp"
#include "features/gallery/state.hpp"
#include "features/letterbox/state.hpp"
#include "features/overlay/state.hpp"
#include "features/photography/state.hpp"
#include "features/preview/state.hpp"
#include "features/recording/state.hpp"
#include "features/screenshot/state.hpp"
#include "features/settings/state.hpp"
#include "features/update/state.hpp"
#include "features/window_control/state.hpp"
#include "ui/context_menu/state.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/notification_window/state.hpp"
#include "ui/photography_panel/state.hpp"
#include "ui/shared_render_resources/state.hpp"
#include "ui/tray_icon/state.hpp"

namespace Core::State {

AppState::AppState()
    : rpc(std::make_unique<Core::RPC::State::RpcState>()),
      async(std::make_unique<Core::Async::State::AsyncState>()),
      dialog_service(std::make_unique<Core::DialogService::State::DialogServiceState>()),
      events(std::make_unique<Core::Events::State::EventsState>()),
      i18n(std::make_unique<Core::I18n::State::I18nState>()),
      webview(std::make_unique<Core::WebView::State::WebViewState>()),
      runtime_info(std::make_unique<Core::State::RuntimeInfo::RuntimeInfoState>()),
      database(std::make_unique<Core::Database::State::DatabaseState>()),
      http_server(std::make_unique<Core::HttpServer::State::HttpServerState>()),
      http_client(std::make_unique<Core::HttpClient::State::HttpClientState>()),
      worker_pool(std::make_unique<Core::WorkerPool::State::WorkerPoolState>()),
      commands(std::make_unique<Core::Commands::State::CommandState>()),
      tasks(std::make_unique<Core::Tasks::State::TaskState>()),
      settings(std::make_unique<Features::Settings::State::SettingsState>()),
      update(std::make_unique<Features::Update::State::UpdateState>()),
      shared_render_resources(
          std::make_unique<UI::SharedRenderResources::State::SharedRenderResourcesState>()),
      floating_window(std::make_unique<UI::FloatingWindow::State::FloatingWindowState>()),
      tray_icon(std::make_unique<UI::TrayIcon::State::TrayIconState>()),
      context_menu(std::make_unique<UI::ContextMenu::State::ContextMenuState>()),
      notification_window(
          std::make_unique<UI::NotificationWindow::State::NotificationWindowState>()),
      photography_panel(std::make_unique<UI::PhotographyPanel::State::PhotographyPanelState>()),
      letterbox(std::make_unique<Features::Letterbox::State::LetterboxState>()),
      gallery(std::make_unique<Features::Gallery::State::GalleryState>()),
      overlay(std::make_unique<Features::Overlay::State::OverlayState>()),
      preview(std::make_unique<Features::Preview::State::PreviewState>()),
      window_control(std::make_unique<Features::WindowControl::State::WindowControlState>()),
      screenshot(std::make_unique<Features::Screenshot::State::ScreenshotState>()),
      recording(std::make_unique<Features::Recording::State::RecordingState>()),
      photography(std::make_unique<Features::Photography::State::PhotographyState>()) {}

AppState::~AppState() = default;

}  // namespace Core::State
