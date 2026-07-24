#include "core/initializer/initializer.hpp"

#include "core/async/async.hpp"
#include "core/commands/registry.hpp"
#include "core/dialog_service/dialog_service.hpp"
#include "core/events/events.hpp"
#include "core/events/registrar.hpp"
#include "core/events/state.hpp"
#include "core/http_client/http_client.hpp"
#include "core/http_server/http_server.hpp"
#include "core/i18n/i18n.hpp"
#include "core/i18n/state.hpp"
#include "core/i18n/types.hpp"
#include "core/initializer/database.hpp"
#include "core/migration/migration.hpp"
#include "core/notifications/notifications.hpp"
#include "core/rpc/registry.hpp"
#include "core/state/app_state.hpp"
#include "core/state/runtime_info.hpp"
#include "core/worker_pool/worker_pool.hpp"
#include "extensions/infinity_nikki/map_service.hpp"
#include "extensions/infinity_nikki/photo_service.hpp"
#include "features/gallery/gallery.hpp"
#include "features/letterbox/state.hpp"
#include "features/recording/recording.hpp"
#include "features/settings/settings.hpp"
#include "features/settings/state.hpp"
#include "features/update/update.hpp"
#include "features/window_control/window_control.hpp"
#include "ui/context_menu/context_menu.hpp"
#include "ui/floating_window/floating_window.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/notification_window/notification_window.hpp"
#include "ui/tray_icon/tray_icon.hpp"
#include "ui/webview_window/webview_window.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace Core::Initializer {

auto apply_language_from_settings(Core::State::AppState& state) -> void {
  if (!state.settings || !state.i18n) {
    Logger().warn("Skip language sync from settings: state is not ready");
    return;
  }

  const auto& locale = state.settings->raw.app.language.current;
  if (auto result = Core::I18n::load_language_by_locale(state, locale); !result) {
    Logger().warn("Failed to load runtime language from settings ('{}'): {}", locale,
                  result.error());
    return;
  }

  Logger().info("Runtime language loaded from settings: {}", locale);
}

auto apply_logger_level_from_settings(Core::State::AppState& state) -> void {
  if (!state.settings) {
    Logger().warn("Skip logger level sync from settings: state is not ready");
    return;
  }

  const auto& level = state.settings->raw.app.logger.level;
  if (auto result = Utils::Logging::set_level(level); !result) {
    Logger().warn("Failed to apply logger level from settings ('{}'): {}", level, result.error());
    return;
  }

  Logger().debug("Runtime logger level loaded from settings: {}", level);
}

// 初始化应用主流程：核心服务先就绪，UI 首屏显示后再启动非首屏功能。
auto initialize_application(Core::State::AppState& state) -> std::expected<void, std::string> {
  try {
    Logger().info("==================================================");
    Logger().info("SpinningMomo startup begin");
    Logger().info("==================================================");

    Core::Events::register_all_handlers(state);

    if (auto result = Core::I18n::initialize(state, Core::I18n::Types::Language::EnUS); !result) {
      return std::unexpected("Failed to initialize i18n: " + result.error());
    }

    auto last_version_result = Core::Migration::get_last_version();
    if (!last_version_result) {
      return std::unexpected("Failed to get last version: " + last_version_result.error());
    }

    const auto last_version = last_version_result.value();
    const auto current_version = state.runtime_info ? state.runtime_info->version : std::string{};
    const bool should_notify_upgrade =
        !current_version.empty() && last_version != "0.0.0.0" &&
        Core::Migration::compare_versions(last_version, current_version) < 0;

    if (auto result = Core::Async::start(state); !result) {
      return std::unexpected("Failed to start async runtime: " + result.error());
    }

    if (auto result = Core::HttpClient::initialize(state); !result) {
      return std::unexpected("Failed to initialize HTTP client: " + result.error());
    }

    if (auto result = Core::WorkerPool::start(state); !result) {
      return std::unexpected("Failed to start worker pool: " + result.error());
    }

    if (auto result = Core::DialogService::start(state); !result) {
      return std::unexpected("Failed to start dialog service: " + result.error());
    }

    Core::RPC::Registry::register_all_endpoints(state);

    if (auto result = Core::HttpServer::initialize(state); !result) {
      return std::unexpected("Failed to initialize HTTP server: " + result.error());
    }

    if (auto db_result = Core::Initializer::Database::initialize_database(state); !db_result) {
      return std::unexpected("Failed to initialize database: " + db_result.error());
    }

    if (!Core::Migration::run_migration_if_needed(state)) {
      return std::unexpected("Application migration failed. Please check logs for details.");
    }

    if (auto settings_result = Features::Settings::initialize(state); !settings_result) {
      return std::unexpected("Failed to initialize settings: " + settings_result.error());
    }

    // 将后端 i18n 语言与 settings 对齐，确保原生浮窗/通知文案一致
    apply_language_from_settings(state);
    apply_logger_level_from_settings(state);

    // 从 settings 同步 letterbox 启用状态
    state.letterbox->enabled = state.settings->raw.features.letterbox.enabled;

    if (auto result = Features::WindowControl::start_center_lock_monitor(state); !result) {
      return std::unexpected("Failed to start window control monitor: " + result.error());
    }

    if (auto update_result = Features::Update::initialize(state); !update_result) {
      return std::unexpected("Failed to initialize update: " + update_result.error());
    }

    // 初始化命令注册表
    Core::Commands::register_builtin_commands(state);
    Logger().info("Command registry initialized with {} commands",
                  Core::Commands::get_all_commands(state).size());

    if (auto result = UI::FloatingWindow::create_window(state); !result) {
      return std::unexpected("Failed to create app window: " + result.error());
    }

    // Set up notify_hwnd for event system wake-up
    state.events->notify_hwnd = state.floating_window->window.hwnd;

    if (auto result = UI::TrayIcon::create(state); !result) {
      return std::unexpected("Failed to create tray icon: " + result.error());
    }

    if (auto result = UI::ContextMenu::initialize(state); !result) {
      return std::unexpected("Failed to initialize tray menu: " + result.error());
    }

    if (auto result = UI::NotificationWindow::initialize(state); !result) {
      return std::unexpected("Failed to initialize notification window: " + result.error());
    }

    // 到这里为止，悬浮窗首绘所需的配置、文案、命令和原生 UI 资源都已就绪。
    // 先显示启动 UI，避免 Gallery 目录探测、远程根检查等非首屏工作阻塞用户看到窗口。
    const bool should_open_onboarding =
        Features::Settings::should_show_onboarding(state.settings->raw);
    if (should_open_onboarding) {
      Logger().info("Onboarding required, attempting to open main UI window");
      UI::WebViewWindow::activate_window(state);
    } else {
      // 默认显示悬浮窗
      UI::FloatingWindow::show_window(state);
    }

    if (should_notify_upgrade) {
      auto text_it = state.i18n->texts.find("message.app_updated_to_prefix");
      if (text_it != state.i18n->texts.end()) {
        Core::Notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                               text_it->second + current_version);
      } else {
        Logger().warn("Skip upgrade notification: i18n text is missing");
      }
    }

    Core::Commands::install_keyboard_keepalive_hook(state);

    // 注册所有命令的热键
    Core::Commands::register_all_hotkeys(state, state.floating_window->window.hwnd);

    // 以下功能服务影响具体能力是否可用，但不应阻塞首屏出现。
    if (auto result = Features::Recording::initialize(state); !result) {
      return std::unexpected(result.error());
    }

    auto register_gallery_extensions = [](Core::State::AppState& app_state) {
      Extensions::InfinityNikki::MapService::register_from_settings(app_state);
      Extensions::InfinityNikki::PhotoService::register_from_settings(app_state);
    };
    if (auto result = Features::Gallery::initialize(state, std::move(register_gallery_extensions));
        !result) {
      return std::unexpected("Failed to initialize gallery: " + result.error());
    }

    Logger().info("==================================================");
    Logger().info("SpinningMomo startup ready");
    Logger().info("==================================================");

    // 按设置自动检查更新（异步，不阻塞启动）
    Features::Update::schedule_startup_auto_update_check(state);

    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception during initialization: " + std::string(e.what()));
  }
}

}  // namespace Core::Initializer
