module;

#include <windows.h>

#include <iostream>

module App;

import std;
import Core.ConfigManager;
import Core.Constants;
import Core.Events;
import Core.State;
import Utils.Logger;
import Utils.String;
import UI.AppWindow;

using UI::AppWindow::create_window;
using UI::AppWindow::CreateParams;
using UI::AppWindow::hide_window;
using UI::AppWindow::register_hotkey;
using UI::AppWindow::set_current_ratio;
using UI::AppWindow::set_current_resolution;
using UI::AppWindow::set_letterbox_enabled;
using UI::AppWindow::set_overlay_enabled;
using UI::AppWindow::set_preview_enabled;
using UI::AppWindow::show_window;
using UI::AppWindow::toggle_visibility;

Application::Application() = default;
Application::~Application() = default;

auto Application::Initialize(HINSTANCE hInstance) -> bool {
  m_h_instance = hInstance;

  try {
    LogSystemInfo();

    // 1. 初始化配置管理器
    m_config_manager = std::make_unique<Core::Config::ConfigManager>();
    if (auto init_result = m_config_manager->Initialize(); !init_result) {
      Logger().error("Failed to initialize config manager: {}", init_result.error());
      return false;
    }

    // 2. 加载所有配置
    if (auto load_result = m_config_manager->LoadAllConfigs(); !load_result) {
      Logger().error("Failed to load configurations: {}", load_result.error());
      return false;
    }

    // 3. 创建事件分发器
    m_event_dispatcher = std::make_shared<Core::Events::EventDispatcher>();

    // 4. 注册事件处理器
    RegisterEventHandlers();

    // 5. 创建AppState
    m_app_state = std::make_unique<Core::State::AppState>();
    m_app_state->window.instance = m_h_instance;
    m_app_state->event_dispatcher = m_event_dispatcher;

    // 6. 从配置中获取数据并传递给AppWindow
    // 注意：这里我们暂时用测试数据，后续需要替换为从配置加载的真实数据
    std::vector<Common::Types::RatioPreset> ratios;
    std::vector<Common::Types::ResolutionPreset> resolutions;
    ratios.emplace_back(L"16:9", 16.0 / 9.0);
    ratios.emplace_back(L"4:3", 4.0 / 3.0);
    resolutions.emplace_back(L"1080p", 1920, 1080);
    resolutions.emplace_back(L"720p", 1280, 720);

    const auto& strings = Constants::ZH_CN;  // 同样，后续应根据语言配置选择

    // 创建窗口参数
    CreateParams params{.ratios = std::span(ratios),
                        .resolutions = std::span(resolutions),
                        .strings = strings,
                        .current_ratio_index = 0,
                        .current_resolution_index = 0,
                        .preview_enabled = false,
                        .overlay_enabled = false,
                        .letterbox_enabled = false,
                        .event_dispatcher = m_event_dispatcher};

    // 创建窗口
    if (auto result = create_window(*m_app_state, params); !result) {
      Logger().error("Failed to create app window: {}", Utils::String::ToUtf8(result.error()));
      return false;
    }

    // 默认显示窗口
    show_window(*m_app_state);

    // 注册热键
    if (auto hotkey_config = m_config_manager->GetHotkeyConfig(); hotkey_config) {
      register_hotkey(*m_app_state, hotkey_config->modifiers, hotkey_config->key);
    } else {
      Logger().warn("Failed to get hotkey config: {}", hotkey_config.error());
    }

    Logger().info("Application initialized successfully");
    return true;

  } catch (const std::exception& e) {
    Logger().error("Exception during initialization: {}", e.what());
    return false;
  }
}

auto Application::Run() -> int {
  MSG msg{};
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    // 处理自定义事件
    if (m_event_dispatcher) {
      m_event_dispatcher->ProcessEvents();
    }
  }
  return static_cast<int>(msg.wParam);
}

auto Application::RegisterEventHandlers() -> void {
  using namespace Core::Events;

  // 注册比例改变事件处理器
  m_event_dispatcher->Subscribe(EventType::RatioChanged, [this](const Event& event) {
    auto data = std::any_cast<RatioChangeData>(event.data);
    Logger().info("Ratio changed");
    // 更新AppWindow显示
    if (m_app_state) {
      set_current_ratio(*m_app_state, data.index);
    }
  });

  // 注册功能开关事件处理器
  m_event_dispatcher->Subscribe(EventType::ToggleFeature, [this](const Event& event) {
    auto data = std::any_cast<FeatureToggleData>(event.data);
    Logger().info("Feature toggled");

    if (m_app_state) {
      switch (data.feature) {
        case FeatureType::Preview:
          set_preview_enabled(*m_app_state, data.enabled);
          break;
        case FeatureType::Overlay:
          set_overlay_enabled(*m_app_state, data.enabled);
          break;
        case FeatureType::Letterbox:
          set_letterbox_enabled(*m_app_state, data.enabled);
          break;
      }
    }
  });

  // 注册分辨率改变事件处理器
  m_event_dispatcher->Subscribe(EventType::ResolutionChanged, [this](const Event& event) {
    auto data = std::any_cast<ResolutionChangeData>(event.data);
    Logger().info("Resolution changed");
    // 更新AppWindow显示
    if (m_app_state) {
      set_current_resolution(*m_app_state, data.index);
    }
  });

  // 注册系统命令事件处理器
  m_event_dispatcher->Subscribe(EventType::SystemCommand, [this](const Event& event) {
    auto command = std::any_cast<std::string>(event.data);
    Logger().info("System command received");

    if (command == "toggle_visibility") {
      if (m_app_state) {
        toggle_visibility(*m_app_state);
      }
    }
  });

  // 注册窗口动作事件处理器
  m_event_dispatcher->Subscribe(EventType::WindowAction, [this](const Event& event) {
    auto action = std::any_cast<WindowAction>(event.data);
    Logger().info("Window action triggered");

    switch (action) {
      case WindowAction::Capture:
        // TODO: 实现截图功能
        break;
      case WindowAction::Screenshot:
        // TODO: 打开相册
        break;
      case WindowAction::Reset:
        // TODO: 重置窗口
        break;
      case WindowAction::Close:
        if (m_app_state) {
          hide_window(*m_app_state);
        }
        break;
      case WindowAction::Exit:
        PostQuitMessage(0);
        break;
    }
  });
}

auto Application::LogSystemInfo() -> void {
  OSVERSIONINFOEXW osvi{};
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

  if (GetVersionExW(reinterpret_cast<LPOSVERSIONINFOW>(&osvi))) {
    Logger().info("OS Version: {}.{}.{}", osvi.dwMajorVersion, osvi.dwMinorVersion,
                  osvi.dwBuildNumber);
  }
}