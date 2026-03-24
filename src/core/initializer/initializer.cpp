module;

module Core.Initializer;

import std;
import Core.Async;
import Core.Commands;
import Core.Commands.State;
import Core.DialogService;
import Core.WorkerPool;
import Core.State;
import Core.State.RuntimeInfo;
import Core.I18n;
import Core.I18n.State;
import Core.I18n.Types;
import Core.HttpServer;
import Core.HttpClient;
import Core.Events;
import Core.Events.State;
import Core.Events.Registrar;
import Core.RPC.Registry;
import Core.Initializer.Database;
import Core.Migration;
import Features.Gallery;
import Features.Gallery.Watcher;
import Features.Settings;
import Features.Settings.State;
import Features.Recording;
import Features.ReplayBuffer;
import Features.ReplayBuffer.State;
import Features.ReplayBuffer.UseCase;
import Features.Update;
import Features.Letterbox.State;
import Features.WindowControl;
import Extensions.InfinityNikki.PhotoService;
import UI.FloatingWindow;
import UI.FloatingWindow.Events;
import UI.FloatingWindow.State;
import UI.WebViewWindow;
import UI.TrayIcon;
import UI.ContextMenu;
import Utils.Logger;
import Vendor.Windows;

namespace Core::Initializer {

auto post_startup_notification(Core::State::AppState& state, const std::string& message) -> void {
  if (!state.events || !state.i18n) {
    Logger().warn("Skip startup notification: state is not ready");
    return;
  }

  auto app_name_it = state.i18n->texts.find("label.app_name");
  if (app_name_it == state.i18n->texts.end()) {
    Logger().warn("Skip startup notification: app name text is missing");
    return;
  }

  Core::Events::post(*state.events, UI::FloatingWindow::Events::NotificationEvent{
                                        .title = app_name_it->second,
                                        .message = message,
                                    });
}

auto apply_language_from_settings(Core::State::AppState& state) -> void {
  if (!state.settings || !state.i18n) {
    Logger().warn("Skip language sync from settings: state is not ready");
    return;
  }

  const auto& locale = state.settings->raw.app.language.current;
  if (auto result = Core::I18n::load_language_by_locale(*state.i18n, locale); !result) {
    Logger().warn("Failed to load runtime language from settings ('{}'): {}", locale,
                  result.error());
    return;
  }

  Logger().info("Runtime language loaded from settings: {}", locale);
}

auto initialize_application(Core::State::AppState& state, Vendor::Windows::HINSTANCE instance)
    -> std::expected<void, std::string> {
  try {
    Logger().info("==================================================");
    Logger().info("SpinningMomo startup begin");
    Logger().info("==================================================");

    Core::Events::register_all_handlers(state);

    if (auto result = Core::I18n::initialize(*state.i18n, Core::I18n::Types::Language::EnUS);
        !result) {
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

    if (auto result = Core::Async::start(*state.async); !result) {
      return std::unexpected("Failed to start async runtime: " + result.error());
    }

    if (auto result = Core::HttpClient::initialize(state); !result) {
      return std::unexpected("Failed to initialize HTTP client: " + result.error());
    }

    if (auto result = Core::WorkerPool::start(*state.worker_pool); !result) {
      return std::unexpected("Failed to start worker pool: " + result.error());
    }

    if (auto result = Core::DialogService::start(*state.dialog_service); !result) {
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

    // 从 settings 同步 letterbox 启用状态
    state.letterbox->enabled = state.settings->raw.features.letterbox.enabled;

    if (auto result = Features::WindowControl::start_center_lock_monitor(state); !result) {
      return std::unexpected("Failed to start window control monitor: " + result.error());
    }

    if (auto update_result = Features::Update::initialize(state); !update_result) {
      return std::unexpected("Failed to initialize update: " + update_result.error());
    }

    // 初始化命令注册表
    Core::Commands::register_builtin_commands(state, state.commands->registry);
    Logger().info("Command registry initialized with {} commands",
                  state.commands->registry.descriptors.size());

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

    if (auto result = Features::Recording::initialize(*state.recording); !result) {
      return std::unexpected(result.error());
    }

    if (auto result = Features::ReplayBuffer::initialize(*state.replay_buffer); !result) {
      return std::unexpected("Failed to initialize replay buffer: " + result.error());
    }

    if (auto gallery_result = Features::Gallery::initialize(state); !gallery_result) {
      return std::unexpected("Failed to initialize gallery: " + gallery_result.error());
    }

    if (auto watcher_restore_result = Features::Gallery::Watcher::restore_watchers_from_db(state);
        !watcher_restore_result) {
      Logger().warn("Gallery watcher registration restore failed: {}",
                    watcher_restore_result.error());
    }

    // Gallery 初始化完成后，先注册无限暖暖目录监听，统一在末尾启动
    Extensions::InfinityNikki::PhotoService::register_from_settings(state);

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
        post_startup_notification(state, text_it->second + current_version);
      } else {
        Logger().warn("Skip upgrade notification: i18n text is missing");
      }
    }

    Core::Commands::install_keyboard_keepalive_hook(state);

    // 注册所有命令的热键
    Core::Commands::register_all_hotkeys(state, state.floating_window->window.hwnd);

    if (auto watcher_start_result = Features::Gallery::Watcher::start_registered_watchers(state);
        !watcher_start_result) {
      Logger().warn("Gallery watcher startup failed: {}", watcher_start_result.error());
    }

    // 按设置自动检查更新（异步，不阻塞启动）
    Features::Update::schedule_startup_auto_update_check(state);

    Logger().info("==================================================");
    Logger().info("SpinningMomo startup ready");
    Logger().info("==================================================");
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception during initialization: " + std::string(e.what()));
  }
}

}  // namespace Core::Initializer
