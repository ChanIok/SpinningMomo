module;

module App;

import std;
import Core.Config.Io;
import Core.Constants;
import Core.Events;
import Core.State;
import Handlers.EventRegistrar;
import Utils.Logger;
import Utils.String;
import UI.AppWindow;
import Vendor.Windows;

Application::Application() = default;
Application::~Application() = default;

auto Application::Initialize(Vendor::Windows::HINSTANCE hInstance) -> bool {
  m_h_instance = hInstance;

  try {
    LogSystemInfo();

    // 1. 创建 AppState
    m_app_state = std::make_unique<Core::State::AppState>();
    m_app_state->window.instance = m_h_instance;

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
    m_app_state->data.strings = &strings;

    auto ratio_data = Core::Config::Io::get_aspect_ratios(m_app_state->config, strings);
    if (!ratio_data.success) {
      Logger().warn("Failed to load aspect ratios: {}",
                    Utils::String::ToUtf8(ratio_data.error_details));
    }
    m_app_state->data.ratios = std::move(ratio_data.ratios);

    auto resolution_data = Core::Config::Io::get_resolution_presets(m_app_state->config, strings);
    if (!resolution_data.success) {
      Logger().warn("Failed to load resolutions: {}",
                    Utils::String::ToUtf8(resolution_data.error_details));
    }
    m_app_state->data.resolutions = std::move(resolution_data.resolutions);

    // 从配置初始化UI状态
    m_app_state->ui.preview_enabled = m_app_state->config.menu.use_floating_window;
    m_app_state->ui.letterbox_enabled = m_app_state->config.letterbox.enabled;

    // 创建窗口
    if (auto result = UI::AppWindow::create_window(*m_app_state); !result) {
      Logger().error("Failed to create app window: {}", result.error());
      return false;
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
  while (Vendor::Windows::GetWindowMessage(&msg, nullptr, 0, 0)) {
    Vendor::Windows::TranslateWindowMessage(&msg);
    Vendor::Windows::DispatchWindowMessage(&msg);

    // 处理自定义事件
    if (m_app_state) {
      Core::Events::process_events(m_app_state->event_bus);
    }
  }
  return static_cast<int>(msg.wParam);
}

auto Application::LogSystemInfo() -> void {
  Vendor::Windows::OSVERSIONINFOEXW osvi{};
  osvi.dwOSVersionInfoSize = sizeof(Vendor::Windows::OSVERSIONINFOEXW);

  if (Vendor::Windows::GetVersionExW(reinterpret_cast<Vendor::Windows::LPOSVERSIONINFOW>(&osvi))) {
    Logger().info("OS Version: {}.{}.{}", osvi.dwMajorVersion, osvi.dwMinorVersion,
                  osvi.dwBuildNumber);
  }
}