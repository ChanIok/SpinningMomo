module;

module App;

import std;
import Core.Initializer;
import Core.RuntimeInfo;
import Core.Shutdown;
import Core.State;
import UI.FloatingWindow.State;
import Utils.Logger;
import Vendor.Windows;

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

    m_app_state->floating_window->window.instance = m_h_instance;

    Core::RuntimeInfo::collect(*m_app_state);

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

  // 消息驱动的事件循环：
  // - WM_APP_PROCESS_EVENTS: 处理异步事件队列
  // - WM_TIMER: 处理通知动画更新（固定 60fps 帧率）
  // 没有任务时 GetMessage 会阻塞，零 CPU 占用
  while (Vendor::Windows::GetWindowMessage(&msg, nullptr, 0, 0)) {
    if (msg.message == Vendor::Windows::kWM_QUIT) {
      return static_cast<int>(msg.wParam);
    }
    Vendor::Windows::TranslateWindowMessage(&msg);
    Vendor::Windows::DispatchWindowMessageW(&msg);
  }

  return static_cast<int>(msg.wParam);
}
