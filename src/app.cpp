module;

module App;

import std;
import Core.Initializer;
import Core.State;
import Core.Async.Runtime;
import Core.Async.State;
import Core.Events;
import Core.I18n.State;
import Core.RpcHandlers.State;
import Core.WebView.State;
import Core.State.AppInfo;
import Features.Settings.State;
import Features.Letterbox.State;
import Features.Letterbox;
import Features.Notifications;
import Features.Notifications.State;
import Features.Overlay.State;
import Features.Overlay;
import Features.Preview.State;
import Features.Preview;
import Features.Screenshot;
import Features.Screenshot.State;
import Features.Settings;
import Features.Updater;
import Features.Updater.State;
import UI.AppWindow;
import UI.AppWindow.State;
import UI.WebViewWindow;
import UI.TrayIcon;
import UI.TrayIcon.State;
import UI.ContextMenu;
import UI.ContextMenu.State;
import Core.WebView;
import Utils.Logger;
import Utils.String;
import Utils.System;
import Vendor.Windows;
import Core.I18n;

Application::Application() = default;
Application::~Application() {
  if (m_app_state) {
    // 检查是否有待处理的更新
    if (m_app_state->updater && m_app_state->updater->pending_update) {
      Logger().info("Executing pending update on program exit");
      Features::Updater::execute_pending_update(*m_app_state);
    }
    Features::Preview::stop_preview(*m_app_state);
    Features::Preview::cleanup_preview(*m_app_state);
    Features::Overlay::stop_overlay(*m_app_state);
    Features::Overlay::cleanup_overlay(*m_app_state);
    if (auto result = Features::Letterbox::shutdown(*m_app_state); !result) {
      Logger().error("Failed to shutdown Letterbox: {}", result.error());
    }
    Features::Screenshot::cleanup_system(*m_app_state);

    // 清理WebView
    UI::WebViewWindow::cleanup(*m_app_state);

    UI::ContextMenu::cleanup(*m_app_state);
    UI::TrayIcon::destroy(*m_app_state);

    Core::Async::stop(*m_app_state->async_runtime);
  }
}

auto Application::Initialize(Vendor::Windows::HINSTANCE hInstance) -> bool {
  m_h_instance = hInstance;

  try {
    // 创建 AppState
    m_app_state = std::make_unique<Core::State::AppState>();
    m_app_state->async_runtime = std::make_unique<Core::Async::State::AsyncRuntimeState>();
    m_app_state->event_bus = std::make_unique<Core::Events::EventBus>();
    m_app_state->i18n = std::make_unique<Core::I18n::State::I18nState>();
    m_app_state->rpc_handlers = std::make_unique<Core::RpcHandlers::State::RpcHandlerState>();
    m_app_state->webview = std::make_unique<Core::WebView::State::WebViewState>();
    m_app_state->app_info = std::make_unique<Core::State::AppInfo::AppInfoState>();
    m_app_state->settings = std::make_unique<Features::Settings::State::SettingsState>();

    // 初始化UI状态
    m_app_state->app_window = std::make_unique<UI::AppWindow::State::AppWindowState>();
    m_app_state->tray_icon = std::make_unique<UI::TrayIcon::State::TrayIconState>();
    m_app_state->context_menu = std::make_unique<UI::ContextMenu::State::ContextMenuState>();

    // 初始化功能模块状态
    m_app_state->letterbox = std::make_unique<Features::Letterbox::State::LetterboxState>();
    m_app_state->notifications =
        std::make_unique<Features::Notifications::State::NotificationSystemState>();
    m_app_state->overlay = std::make_unique<Features::Overlay::State::OverlayState>();
    m_app_state->preview = std::make_unique<Features::Preview::State::PreviewState>();
    m_app_state->screenshot = std::make_unique<Features::Screenshot::State::ScreenshotState>();
    m_app_state->updater = std::make_unique<Features::Updater::State::UpdateState>();

    m_app_state->app_window->window.instance = m_h_instance;

    LogSystemInfo();

    // 测试嵌入式多语言系统
    Logger().info("=== Testing Embedded I18n System ===");

    // 初始化I18n系统
    if (auto result = Core::I18n::initialize(*m_app_state->i18n, Core::I18n::Types::Language::ZhCN);
        !result) {
      Logger().error("Failed to initialize I18n system: {}", result.error());
    } else {
      Logger().info("I18n system initialized successfully with default Chinese");

      // 直接访问texts字段测试默认中文
      const auto& app_name = m_app_state->i18n->texts.label.app_name;
      Logger().info("Default app name (Chinese): {}", app_name);
    }

    Logger().info("=== I18n Testing Complete ===");

    // 调用统一的初始化器
    if (auto result = Core::Initializer::initialize_application(*m_app_state, m_h_instance);
        !result) {
      Logger().error("Failed to initialize application: {}", result.error());
      return false;
    }

    return true;

  } catch (const std::exception& e) {
    Logger().error("Exception during initialization: {}", e.what());
    return false;
  }
}

auto Application::Run() -> int {
  Vendor::Windows::MSG msg{};
  const auto timeout = 10;

  while (true) {
    // 等待消息或超时
    Vendor::Windows::MsgWaitForMultipleObjectsEx(0, nullptr, timeout,
                                                 Vendor::Windows::QS_ALLINPUT_t,
                                                 Vendor::Windows::MWMO_INPUTAVAILABLE_t);

    // 处理所有挂起的消息
    while (Vendor::Windows::PeekMessageW(&msg, nullptr, 0, 0, Vendor::Windows::PM_REMOVE_t)) {
      if (msg.message == Vendor::Windows::WM_QUIT_t) {
        return static_cast<int>(msg.wParam);
      }
      Vendor::Windows::TranslateWindowMessage(&msg);
      Vendor::Windows::DispatchWindowMessageW(&msg);
    }

    // 在处理完消息或超时后，运行我们的更新逻辑
    if (m_app_state) {
      Core::Events::process_events(*m_app_state->event_bus);

      // 更新通知系统
      Features::Notifications::update_notifications(*m_app_state);
    }
  }
}

auto Application::LogSystemInfo() -> void {
  // 使用 Utils::System 模块获取系统版本信息
  if (auto version_result = Utils::System::get_windows_version()) {
    const auto& version = version_result.value();
    Logger().info("OS Version: {}.{}.{}", version.major_version, version.minor_version,
                  version.build_number);

    // 填充系统信息到 AppInfoState
    if (m_app_state && m_app_state->app_info) {
      m_app_state->app_info->os_major_version = version.major_version;
      m_app_state->app_info->os_minor_version = version.minor_version;
      m_app_state->app_info->os_build_number = version.build_number;
      m_app_state->app_info->os_name = Utils::System::get_windows_name(version);

      // 设置捕获支持状态 (Windows 10 1903 build 18362 or later)
      m_app_state->app_info->is_capture_supported =
          (version.major_version > 10) ||
          (version.major_version == 10 && version.build_number >= 18362);
      Logger().info("Is capture supported: {}", m_app_state->app_info->is_capture_supported);
    }
  } else {
    Logger().error("Failed to get OS version: {}", version_result.error());
  }
}