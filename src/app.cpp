module;

module App;

import std;
import Core.Config.Io;
import Core.Constants;
import Core.Events;
import Core.State;
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
    RegisterEventHandlers();

    // 4. 从配置中获取数据并传递给AppWindow
    // TODO: 根据语言配置选择字符串
    const auto& strings = Constants::ZH_CN;

    auto ratio_data = Core::Config::Io::get_aspect_ratios(m_app_state->config, strings);
    if (!ratio_data.success) {
      Logger().warn("Failed to load aspect ratios: {}",
                    Utils::String::ToUtf8(ratio_data.error_details));
    }

    auto resolution_data = Core::Config::Io::get_resolution_presets(m_app_state->config, strings);
    if (!resolution_data.success) {
      Logger().warn("Failed to load resolutions: {}",
                    Utils::String::ToUtf8(resolution_data.error_details));
    }

    // 创建窗口参数
    UI::AppWindow::CreateParams params{
        .ratios = std::span(ratio_data.ratios),
        .resolutions = std::span(resolution_data.resolutions),
        .strings = strings,
        .current_ratio_index = m_app_state->ui.current_ratio_index,            // 初始值为0
        .current_resolution_index = m_app_state->ui.current_resolution_index,  // 初始值为0
        .preview_enabled = m_app_state->config.menu.use_floating_window,       // 示例，具体逻辑待定
        .overlay_enabled = m_app_state->ui.overlay_enabled,                    // 初始值为false
        .letterbox_enabled = m_app_state->config.letterbox.enabled};

    // 创建窗口
    if (auto result = UI::AppWindow::create_window(*m_app_state, params); !result) {
      Logger().error("Failed to create app window: {}", Utils::String::ToUtf8(result.error()));
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

auto Application::RegisterEventHandlers() -> void {
  using namespace Core::Events;

  // 注册比例改变事件处理器
  subscribe(m_app_state->event_bus, EventType::RatioChanged, [this](const Event& event) {
    if (!m_app_state) return;
    auto data = std::any_cast<RatioChangeData>(event.data);
    Logger().info("Ratio changed to index {}", data.index);
    UI::AppWindow::set_current_ratio(*m_app_state, data.index);
    // 注意：这里可能需要更新配置文件，但通常比例选择是临时状态，不保存
  });

  // 注册功能开关事件处理器
  subscribe(m_app_state->event_bus, EventType::ToggleFeature, [this](const Event& event) {
    if (!m_app_state) return;
    auto data = std::any_cast<FeatureToggleData>(event.data);
    Logger().info("Feature toggled");

    bool config_changed = false;
    switch (data.feature) {
      case FeatureType::Preview:
        UI::AppWindow::set_preview_enabled(*m_app_state, data.enabled);
        // m_app_state->config.menu.use_floating_window = data.enabled; // 假设预览就是浮动窗口
        // config_changed = true;
        break;
      case FeatureType::Overlay:
        UI::AppWindow::set_overlay_enabled(*m_app_state, data.enabled);
        // Overlay 通常是临时状态，不写入配置
        break;
      case FeatureType::Letterbox:
        UI::AppWindow::set_letterbox_enabled(*m_app_state, data.enabled);
        m_app_state->config.letterbox.enabled = data.enabled;
        config_changed = true;
        break;
    }

    if (config_changed) {
      if (auto result = Core::Config::Io::save(m_app_state->config); !result) {
        Logger().error("Failed to save config: {}", result.error());
      }
    }
  });

  // 注册分辨率改变事件处理器
  subscribe(m_app_state->event_bus, EventType::ResolutionChanged, [this](const Event& event) {
    if (!m_app_state) return;
    auto data = std::any_cast<ResolutionChangeData>(event.data);
    Logger().info("Resolution changed to index {}", data.index);
    UI::AppWindow::set_current_resolution(*m_app_state, data.index);
    // 注意：分辨率选择也可能是临时状态
  });

  // 注册系统命令事件处理器
  subscribe(m_app_state->event_bus, EventType::SystemCommand, [this](const Event& event) {
    auto command = std::any_cast<std::string>(event.data);
    Logger().info("System command received");

    if (command == "toggle_visibility") {
      if (m_app_state) {
        UI::AppWindow::toggle_visibility(*m_app_state);
      }
    }
  });

  // 注册窗口动作事件处理器
  subscribe(m_app_state->event_bus, EventType::WindowAction, [this](const Event& event) {
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
          UI::AppWindow::hide_window(*m_app_state);
        }
        break;
      case WindowAction::Exit:
        Vendor::Windows::PostQuitMessage(0);
        break;
    }
  });
}

auto Application::LogSystemInfo() -> void {
  Vendor::Windows::OSVERSIONINFOEXW osvi{};
  osvi.dwOSVersionInfoSize = sizeof(Vendor::Windows::OSVERSIONINFOEXW);

  if (Vendor::Windows::GetVersionExW(reinterpret_cast<Vendor::Windows::LPOSVERSIONINFOW>(&osvi))) {
    Logger().info("OS Version: {}.{}.{}", osvi.dwMajorVersion, osvi.dwMinorVersion,
                  osvi.dwBuildNumber);
  }
}