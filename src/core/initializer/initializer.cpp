module;

module Core.Initializer;

import std;
import Core.Async.Runtime;
import Core.Events;
import Core.State;
import Core.HttpServer;
import Core.Events.Registrar;
import Core.Initializer.Database;
import Features.Notifications;
import Features.Settings;
import Features.Settings.State;
import Features.Updater;
import Core.RPC.Registry;
import UI.AppWindow;
import UI.AppWindow.State;
import UI.TrayIcon;
import UI.ContextMenu;
import Vendor.Windows;
import Utils.Logger;
import Utils.String;

namespace Core::Initializer {

auto initialize_application(Core::State::AppState& state, Vendor::Windows::HINSTANCE instance)
    -> std::expected<void, std::string> {
  try {
    // 1. 启动异步运行时（用于RPC处理）
    if (auto result = Core::Async::start(*state.async_runtime); !result) {
      return std::unexpected("Failed to start async runtime: " + result.error());
    }

    // 2. 初始化HTTP服务器
    if (auto result = Core::HttpServer::initialize(state); !result) {
      Logger().warn("Failed to initialize HTTP server: {}", result.error());
      // HTTP服务器失败不应该阻止应用启动
    }

    // 3. 注册事件处理器
    Core::Events::register_all_handlers(state);

    // 4. 初始化数据库
    if (auto db_result = Core::Initializer::Database::initialize_database(state); !db_result) {
      Logger().error("Failed to initialize database: {}", db_result.error());
      return std::unexpected("Failed to initialize database: " + db_result.error());
    }

    // 5. 初始化 settings 模块
    if (auto settings_result = Features::Settings::initialize(state); !settings_result) {
      Logger().error("Failed to initialize settings: {}", settings_result.error());
      return std::unexpected("Failed to initialize settings: " + settings_result.error());
    }

    // 4.1. 初始化 updater 模块
    if (auto updater_result = Features::Updater::initialize(state); !updater_result) {
      Logger().warn("Failed to initialize updater: {}", updater_result.error());
      // 不影响应用启动
    }

    // 5.3. 注册新的RPC端点
    Core::RPC::Registry::register_all_endpoints(state);

    // 7. 创建窗口
    if (auto result = UI::AppWindow::create_window(state); !result) {
      return std::unexpected("Failed to create app window: " + result.error());
    }

    // 9. 创建托盘图标
    if (auto result = UI::TrayIcon::create(state); !result) {
      Logger().warn("Failed to create tray icon: {}", result.error());
      // This might not be a fatal error, so we just log a warning.
    }

    // 10. 初始化托盘菜单
    if (auto result = UI::ContextMenu::initialize(state); !result) {
      Logger().warn("Failed to initialize tray menu: {}", result.error());
      // 托盘菜单初始化失败，但不影响应用程序运行
    }

    // 14. 默认显示窗口
    UI::AppWindow::show_window(state);

    // 15. 注册热键（从settings中读取配置）
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