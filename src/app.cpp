module;

module App;

import std;
import Core.Initializer;
import Core.State;
import Core.Events;
import Core.State.AppInfo;
import Features.Letterbox;
import Features.Notifications;
import Features.Overlay;
import Features.Preview;
import Features.Screenshot;
import Features.Settings;
import Features.Updater;
import UI.AppWindow;
import UI.AppWindow.State;
import UI.WebViewWindow;
import UI.TrayIcon;
import UI.ContextMenu;
import Core.WebView;
import Core.HttpServer;
import Core.Shutdown;
import Utils.Logger;
import Utils.String;
import Utils.System;
import Vendor.Windows;
import Core.I18n;
import Core.I18n.Types;

Application::Application() = default;
Application::~Application() {
  if (m_app_state) {
    Core::Shutdown::shutdown_application(*m_app_state);
  }
}

auto Application::Initialize(Vendor::Windows::HINSTANCE hInstance) -> bool {
  m_h_instance = hInstance;

  try {
    // 创建 AppState, 其构造函数会自动初始化所有子状态
    m_app_state = std::make_unique<Core::State::AppState>();

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
    Vendor::Windows::MsgWaitForMultipleObjectsEx(0, nullptr, timeout, Vendor::Windows::kQS_ALLINPUT,
                                                 Vendor::Windows::kMWMO_INPUTAVAILABLE);

    // 处理所有挂起的消息
    while (Vendor::Windows::PeekMessageW(&msg, nullptr, 0, 0, Vendor::Windows::kPM_REMOVE)) {
      if (msg.message == Vendor::Windows::kWM_QUIT) {
        return static_cast<int>(msg.wParam);
      }
      Vendor::Windows::TranslateWindowMessage(&msg);
      Vendor::Windows::DispatchWindowMessageW(&msg);
    }

    // 在处理完消息或超时后，运行我们的更新逻辑
    if (m_app_state) {
      Core::Events::process_events(*m_app_state->events);

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