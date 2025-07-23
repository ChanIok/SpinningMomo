module;

module Core.Initializer;

import std;
import Core.Async.Runtime;
import Core.Constants;
import Core.Events;
import UI.AppWindow.State;
import Core.State;
import Core.WebView;
import Core.WebView.State;
import Handlers.EventRegistrar;
import Features.Letterbox;
import Features.Notifications;
import Features.Overlay;
import Features.Preview.Window;
import Features.Screenshot;
import Features.Settings;
import Features.Settings.Rpc;
import Utils.Logger;
import Utils.String;
import UI.AppWindow;
import UI.WebViewWindow;
import UI.TrayIcon;
import UI.ContextMenu;
import Vendor.Windows;

namespace Core::Initializer {

auto initialize_application(Core::State::AppState& state, Vendor::Windows::HINSTANCE instance)
    -> std::expected<void, std::string> {
  try {
    // 1. 启动异步运行时（用于RPC处理）
    if (auto result = Core::Async::start(*state.async_runtime); !result) {
      return std::unexpected("Failed to start async runtime: " + result.error());
    }

    // 3. 注册事件处理器
    Handlers::register_all_handlers(state);

    // 4. 初始化 settings 模块
    if (auto settings_result = Features::Settings::initialize(state); !settings_result) {
      Logger().warn("Failed to initialize settings: {}", settings_result.error());
      // 不影响应用启动
    }

    // 5. 注册 Settings RPC 处理器
    Features::Settings::Rpc::register_handlers(state);

    // 注意：比例和分辨率数据现在由settings模块管理
    // 在Features::Settings::initialize(state)中已经处理了数据的加载和计算

    // 7. 创建窗口
    if (auto result = UI::AppWindow::create_window(state); !result) {
      return std::unexpected("Failed to create app window: " + result.error());
    }

    // 8. 创建独立的WebView窗口
    if (auto result = UI::WebViewWindow::create(state); !result) {
      Logger().warn("Failed to create WebView window: {}", result.error());
      // WebView功能不可用，但应用继续运行
    } else {
      // 初始化WebView内容
      if (auto webview_result = Core::WebView::initialize(state, state.webview->window.parent_hwnd);
          !webview_result) {
        Logger().error("Failed to initialize WebView: {}", webview_result.error());
        // WebView初始化失败，但应用继续运行
      } else {
        Logger().info("WebView initialization started (async)");
      }
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

    // 11. 初始化预览系统
    if (auto preview_result = Features::Preview::Window::initialize_preview(
            state, instance, state.app_window->window.hwnd);
        !preview_result) {
      Logger().warn("Failed to initialize preview system");
      // 预览功能不可用，但应用继续运行
    }

    // 12. 初始化overlay系统
    if (auto overlay_result =
            Features::Overlay::initialize_overlay(state, instance, state.app_window->window.hwnd);
        !overlay_result) {
      Logger().warn("Failed to initialize overlay system: {}", overlay_result.error());
      // overlay功能不可用，但应用继续运行
    }

    // 13. 初始化letterbox系统
    if (auto letterbox_result = Features::Letterbox::initialize(state, instance);
        !letterbox_result) {
      Logger().warn("Failed to initialize letterbox system: {}", letterbox_result.error());
      // letterbox功能不可用，但应用继续运行
    }

    // 14. 默认显示窗口
    UI::AppWindow::show_window(state);

    // 15. 注册热键（暂时硬编码）
    // TODO: 等待settings设计完成后，从settings中读取热键配置
    UI::AppWindow::register_hotkey(state, 0, 0); // 硬编码为无热键

    Logger().info("Application initialized successfully");
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Exception during initialization: " + std::string(e.what()));
  }
}

}  // namespace Core::Initializer