module;

#include <windows.h>

#include <iostream>

module App;

import std;
import Core.ConfigManager;
import Core.Constants;
import Utils.Logger;
import Utils.String;
import UI.AppWindow;

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

    // 3. 创建并初始化AppWindow
    m_app_window = std::make_unique<AppWindow>(m_h_instance);

    // 4. 从配置中获取数据并传递给AppWindow
    // 注意：这里我们暂时用测试数据，后续需要替换为从配置加载的真实数据
    std::vector<Common::Types::RatioPreset> ratios;
    std::vector<Common::Types::ResolutionPreset> resolutions;
    ratios.emplace_back(L"16:9", 16.0 / 9.0);
    ratios.emplace_back(L"4:3", 4.0 / 3.0);
    resolutions.emplace_back(L"1080p", 1920, 1080);
    resolutions.emplace_back(L"720p", 1280, 720);

    const auto& strings = Constants::ZH_CN;  // 同样，后续应根据语言配置选择

    // 传递参数给AppWindow的Create方法
    if (auto result = m_app_window->Create(std::span(ratios), std::span(resolutions), strings, 0, 0,
                                           false, false, false);
        !result) {
      Logger().error("Failed to create app window: {}", Utils::String::ToUtf8(result.error()));
      return false;
    }

    // 默认显示窗口
    m_app_window->Show();

    // 5. AppWindow 自己处理热键注册
    // 我们将从配置中读取的热键信息传递给它
    if (auto hotkey_config = m_config_manager->GetHotkeyConfig(); hotkey_config) {
      m_app_window->RegisterHotkey(hotkey_config->modifiers, hotkey_config->key);
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
  }
  return static_cast<int>(msg.wParam);
}

auto Application::LogSystemInfo() -> void {
  OSVERSIONINFOEXW osvi{};
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

  if (GetVersionExW(reinterpret_cast<LPOSVERSIONINFOW>(&osvi))) {
    Logger().info("OS Version: {}.{}.{}", osvi.dwMajorVersion, osvi.dwMinorVersion,
                  osvi.dwBuildNumber);
  }
}