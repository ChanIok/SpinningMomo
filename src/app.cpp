#include "app.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/initializer/initializer.hpp"
#include "core/runtime_info/runtime_info.hpp"
#include "core/shutdown/shutdown.hpp"
#include "core/state/app_state.hpp"
#include "ui/floating_window/state.hpp"
#include "utils/logger/logger.hpp"

Application::Application() = default;

Application::~Application() { core::shutdown::shutdown_application(m_app_state); }

auto Application::Initialize(HINSTANCE hInstance) -> bool {
  try {
    // 创建 AppState, 其构造函数会自动初始化所有子状态
    m_app_state.floating_window->window.instance = hInstance;

    core::runtime_info::collect(m_app_state);

    // 调用统一的初始化器
    if (auto result = core::initializer::initialize_application(m_app_state); !result) {
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
  MSG msg{};

  // 消息驱动的事件循环：
  // - WM_APP_PROCESS_EVENTS: 处理异步事件队列
  // - WM_TIMER: 处理通知动画更新（固定 60fps 帧率）
  // 没有任务时 GetMessage 会阻塞，零 CPU 占用
  while (::GetMessageW(&msg, nullptr, 0, 0)) {
    if (msg.message == WM_QUIT) {
      return static_cast<int>(msg.wParam);
    }
    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);
  }

  return static_cast<int>(msg.wParam);
}
