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
import Features.Settings.State;
import Features.Letterbox.State;
import Features.Letterbox;
import Features.Notifications;
import Features.Notifications.State;
import Features.Overlay.State;
import Features.Overlay;
import Features.Preview.State;
import Features.Preview.Window;
import Features.Screenshot;
import Features.Screenshot.State;
import Features.Settings;
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
import Vendor.Windows;
import Core.I18n;

Application::Application() = default;
Application::~Application() {
  if (m_app_state) {
    Features::Preview::Window::cleanup_preview(*m_app_state);
    Features::Overlay::cleanup_overlay(*m_app_state);
    if (auto result = Features::Letterbox::shutdown(*m_app_state); !result) {
      Logger().error("Failed to shutdown Letterbox: {}", result.error());
    }
    Features::Screenshot::cleanup_system(*m_app_state->screenshot);

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
    LogSystemInfo();

    // 创建 AppState
    m_app_state = std::make_unique<Core::State::AppState>();
    m_app_state->async_runtime = std::make_unique<Core::Async::State::AsyncRuntimeState>();
    m_app_state->event_bus = std::make_unique<Core::Events::EventBus>();
    m_app_state->i18n = std::make_unique<Core::I18n::State::I18nState>();
    m_app_state->rpc_handlers = std::make_unique<Core::RpcHandlers::State::RpcHandlerState>();
    m_app_state->webview = std::make_unique<Core::WebView::State::WebViewState>();
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

    m_app_state->app_window->window.instance = m_h_instance;

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
  const auto timeout = 16;  // ~60 FPS

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
  Vendor::Windows::OSVERSIONINFOEXW osvi{};
  osvi.dwOSVersionInfoSize = sizeof(Vendor::Windows::OSVERSIONINFOEXW);

  if (Vendor::Windows::GetVersionExW(reinterpret_cast<Vendor::Windows::LPOSVERSIONINFOW>(&osvi))) {
    Logger().info("OS Version: {}.{}.{}", osvi.dwMajorVersion, osvi.dwMinorVersion,
                  osvi.dwBuildNumber);
  }
}