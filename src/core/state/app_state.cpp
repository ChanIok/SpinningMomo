#include "core/state/app_state.hpp"

#include "vendor/std.hpp"

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

namespace core {

AppState::AppState()
    : rpc(std::make_unique<core::rpc::RpcState>()),
      async(std::make_unique<core::async::AsyncState>()),
      dialog_service(std::make_unique<core::dialog_service::DialogServiceState>()),
      events(std::make_unique<core::events::EventsState>()),
      i18n(std::make_unique<core::i18n::I18nState>()),
      webview(std::make_unique<core::webview::WebViewState>()),
      runtime_info(std::make_unique<core::runtime_info::RuntimeInfoState>()),
      database(std::make_unique<core::database::DatabaseState>()),
      http_server(std::make_unique<core::http_server::HttpServerState>()),
      http_client(std::make_unique<core::http_client::HttpClientState>()),
      worker_pool(std::make_unique<core::worker_pool::WorkerPoolState>()),
      commands(std::make_unique<core::commands::CommandState>()),
      tasks(std::make_unique<core::tasks::TaskState>()),
      settings(std::make_unique<features::settings::SettingsState>()),
      update(std::make_unique<features::update::UpdateState>()),
      shared_render_resources(
          std::make_unique<ui::shared_render_resources::SharedRenderResourcesState>()),
      floating_window(std::make_unique<ui::floating_window::FloatingWindowState>()),
      tray_icon(std::make_unique<ui::tray_icon::TrayIconState>()),
      context_menu(std::make_unique<ui::context_menu::ContextMenuState>()),
      notification_window(std::make_unique<ui::notification_window::NotificationWindowState>()),
      photography_panel(std::make_unique<ui::photography_panel::PhotographyPanelState>()),
      letterbox(std::make_unique<features::letterbox::LetterboxState>()),
      gallery(std::make_unique<features::gallery::GalleryState>()),
      overlay(std::make_unique<features::overlay::OverlayState>()),
      preview(std::make_unique<features::preview::PreviewState>()),
      window_control(std::make_unique<features::window_control::WindowControlState>()),
      screenshot(std::make_unique<features::screenshot::ScreenshotState>()),
      recording(std::make_unique<features::recording::RecordingState>()),
      photography(std::make_unique<features::photography::PhotographyState>()) {}

AppState::~AppState() = default;

}  // namespace core
