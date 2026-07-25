#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/events/events.hpp"
#include "core/state/app_state.hpp"

// 主应用程序类
class Application {
 public:
  Application();
  ~Application();

  // 禁用拷贝和移动
  Application(const Application&) = delete;
  auto operator=(const Application&) -> Application& = delete;
  Application(Application&&) = delete;
  auto operator=(Application&&) -> Application& = delete;

  // 初始化
  [[nodiscard]] auto Initialize(HINSTANCE hInstance) -> bool;

  // 运行应用程序
  [[nodiscard]] auto Run() -> int;

 private:
  // 应用状态
  core::AppState m_app_state{};
};
