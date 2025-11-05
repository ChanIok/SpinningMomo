module;

module Core.Initializer;

import std;
import Core.Async;
import Core.WorkerPool;
import Core.State;
import Core.HttpServer;
import Core.Events.Registrar;
import Core.RPC.Registry;
import Core.Initializer.Database;
import Core.Migration;
import Features.Gallery;
import Features.Settings;
import Features.Settings.State;
import Features.Update;
import UI.AppWindow;
import UI.TrayIcon;
import UI.ContextMenu;
import Vendor.Windows;
import Utils.Logger;

namespace Core::Initializer {

auto initialize_application(Core::State::AppState& state, Vendor::Windows::HINSTANCE instance)
    -> std::expected<void, std::string> {
  try {
    Core::Events::register_all_handlers(state);

    if (auto result = Core::Async::start(*state.async); !result) {
      return std::unexpected("Failed to start async runtime: " + result.error());
    }

    // 启动工作线程池
    if (auto result = Core::WorkerPool::start(*state.worker_pool); !result) {
      return std::unexpected("Failed to start worker pool: " + result.error());
    }

    Core::RPC::Registry::register_all_endpoints(state);

    if (auto result = Core::HttpServer::initialize(state); !result) {
      return std::unexpected("Failed to initialize HTTP server: " + result.error());
    }

    if (auto db_result = Core::Initializer::Database::initialize_database(state); !db_result) {
      return std::unexpected("Failed to initialize database: " + db_result.error());
    }

    if (auto settings_result = Features::Settings::initialize(state); !settings_result) {
      return std::unexpected("Failed to initialize settings: " + settings_result.error());
    }

    if (!Core::Migration::run_migration_if_needed(state)) {
      return std::unexpected("Application migration failed. Please check logs for details.");
    }

    if (auto updater_result = Features::Update::initialize(state); !updater_result) {
      return std::unexpected("Failed to initialize updater: " + updater_result.error());
    }

    if (auto result = UI::AppWindow::create_window(state); !result) {
      return std::unexpected("Failed to create app window: " + result.error());
    }

    if (auto result = UI::TrayIcon::create(state); !result) {
      return std::unexpected("Failed to create tray icon: " + result.error());
    }

    if (auto result = UI::ContextMenu::initialize(state); !result) {
      return std::unexpected("Failed to initialize tray menu: " + result.error());
    }

    if (auto gallery_result = Features::Gallery::initialize(state); !gallery_result) {
      return std::unexpected("Failed to initialize gallery: " + gallery_result.error());
    }

    // 默认显示窗口
    UI::AppWindow::show_window(state);

    // 注册热键（从settings中读取配置）
    UI::AppWindow::register_toggle_visibility_hotkey(
        state, state.settings->config.app.hotkey.toggle_visibility.modifiers,
        state.settings->config.app.hotkey.toggle_visibility.key);
    UI::AppWindow::register_screenshot_hotkey(
        state, state.settings->config.app.hotkey.screenshot.modifiers,
        state.settings->config.app.hotkey.screenshot.key);

    Logger().info("Application initialized successfully");
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception during initialization: " + std::string(e.what()));
  }
}

}  // namespace Core::Initializer