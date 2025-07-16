module;

module App;

import std;
import Core.Initializer;
import Core.State;
import Core.Async.Runtime;
import Features.Letterbox;
import Features.Notifications;
import Features.Overlay;
import Features.Preview.Window;
import Features.Screenshot;
import Features.Settings;
import UI.AppWindow;
import UI.WebViewWindow;
import UI.TrayIcon;
import UI.TrayMenu;
import Core.WebView;
import Utils.Logger;
import Vendor.Windows;

Application::Application() = default;
Application::~Application() {
  if (m_app_state) {
    Features::Preview::Window::cleanup_preview(*m_app_state);
    Features::Overlay::stop_overlay(*m_app_state);
    if (auto result = Features::Letterbox::shutdown(*m_app_state); !result) {
      Logger().error("Failed to shutdown Letterbox: {}", result.error());
    }
    Features::Screenshot::cleanup_system(m_app_state->screenshot);

    // 清理WebView
    Core::WebView::shutdown(*m_app_state);
    UI::WebViewWindow::destroy(*m_app_state);

    UI::TrayMenu::cleanup(*m_app_state);
    UI::TrayIcon::destroy(*m_app_state);

    Core::Async::stop(*m_app_state);
  }
}

auto Application::Initialize(Vendor::Windows::HINSTANCE hInstance) -> bool {
  m_h_instance = hInstance;

  try {
    LogSystemInfo();

    // 创建 AppState
    m_app_state = std::make_unique<Core::State::AppState>();
    m_app_state->app_window.window.instance = m_h_instance;

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
      Core::Events::process_events(m_app_state->event_bus);

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