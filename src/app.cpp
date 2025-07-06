module;

module App;

import std;
import Core.Config.Io;
import Core.Constants;
import Core.Events;
import Core.State;
import Handlers.EventRegistrar;
import Features.Letterbox;
import Features.Notifications;
import Features.Overlay;
import Features.Preview.Window;
import Features.Screenshot;
import Utils.Logger;
import Utils.String;
import UI.AppWindow;
import UI.TrayIcon;
import UI.TrayMenu;
import Vendor.Windows;

Application::Application() = default;
Application::~Application() {
  if (m_app_state) {
    Features::Preview::Window::cleanup_preview(*m_app_state);
    Features::Overlay::stop_overlay(*m_app_state);
    Features::Letterbox::shutdown(*m_app_state);
    Features::Screenshot::cleanup_system(m_app_state->screenshot);
    UI::TrayMenu::cleanup(*m_app_state);
    UI::TrayIcon::destroy(*m_app_state);
  }
}

auto Application::Initialize(Vendor::Windows::HINSTANCE hInstance) -> bool {
  m_h_instance = hInstance;

  try {
    LogSystemInfo();

    // 1. 创建 AppState
    m_app_state = std::make_unique<Core::State::AppState>();
    m_app_state->app_window.window.instance = m_h_instance;

    // 2. 初始化配置
    if (auto config_result = Core::Config::Io::initialize(); config_result) {
      m_app_state->config = std::move(config_result.value());
    } else {
      Logger().error("Failed to initialize configuration: {}", config_result.error());
      return false;
    }

    // 3. 注册事件处理器
    Handlers::register_all_handlers(*m_app_state);

    // 4. 从配置中获取数据并填充到 AppState
    const auto& strings =
        (m_app_state->config.language.current_language == Core::Constants::LANG_ZH_CN)
            ? Core::Constants::ZH_CN
            : Core::Constants::EN_US;
    m_app_state->app_window.data.strings = &strings;

    auto ratio_data = Core::Config::Io::get_aspect_ratios(m_app_state->config, strings);
    if (!ratio_data.success) {
      Logger().warn("Failed to load aspect ratios: {}",
                    Utils::String::ToUtf8(ratio_data.error_details));
    }
    m_app_state->app_window.data.ratios = std::move(ratio_data.ratios);

    auto resolution_data = Core::Config::Io::get_resolution_presets(m_app_state->config, strings);
    if (!resolution_data.success) {
      Logger().warn("Failed to load resolutions: {}",
                    Utils::String::ToUtf8(resolution_data.error_details));
    }
    m_app_state->app_window.data.resolutions = std::move(resolution_data.resolutions);

    // 从配置初始化UI状态
    m_app_state->app_window.ui.preview_enabled = m_app_state->config.menu.use_floating_window;
    m_app_state->app_window.ui.letterbox_enabled = m_app_state->config.letterbox.enabled;

    // 创建窗口
    if (auto result = UI::AppWindow::create_window(*m_app_state); !result) {
      Logger().error("Failed to create app window: {}", result.error());
      return false;
    }

    // 创建托盘图标
    if (auto result = UI::TrayIcon::create(*m_app_state); !result) {
      Logger().warn("Failed to create tray icon: {}", result.error());
      // This might not be a fatal error, so we just log a warning.
    }

    // 初始化托盘菜单
    if (auto result = UI::TrayMenu::initialize(*m_app_state); !result) {
      Logger().warn("Failed to initialize tray menu: {}", result.error());
      // 托盘菜单初始化失败，但不影响应用程序运行
    }

    // 初始化预览系统
    if (auto preview_result = Features::Preview::Window::initialize_preview(
            *m_app_state, m_h_instance, m_app_state->app_window.window.hwnd);
        !preview_result) {
      Logger().warn("Failed to initialize preview system");
      // 预览功能不可用，但应用继续运行
    }

    // 初始化overlay系统
    if (auto overlay_result = Features::Overlay::initialize_overlay(
            *m_app_state, m_h_instance, m_app_state->app_window.window.hwnd);
        !overlay_result) {
      Logger().warn("Failed to initialize overlay system: {}", overlay_result.error());
      // overlay功能不可用，但应用继续运行
    }

    // 初始化letterbox系统
    if (auto letterbox_result = Features::Letterbox::initialize(*m_app_state, m_h_instance);
        !letterbox_result) {
      Logger().warn("Failed to initialize letterbox system: {}", letterbox_result.error());
      // letterbox功能不可用，但应用继续运行
    }

    // 默认显示窗口
    UI::AppWindow::show_window(*m_app_state);

    // 注册热键
    UI::AppWindow::register_hotkey(*m_app_state, m_app_state->config.hotkey.modifiers,
                                   m_app_state->config.hotkey.key);

    Logger().info("Application initialized successfully");
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