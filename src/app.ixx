module;

export module App;

import Core.Events;
import Core.State;
import Vendor.Windows;

// 主应用程序类
export class Application {
 public:
  Application();
  ~Application();

  // 禁用拷贝和移动
  Application(const Application&) = delete;
  auto operator=(const Application&) -> Application& = delete;
  Application(Application&&) = delete;
  auto operator=(Application&&) -> Application& = delete;

  // 初始化
  [[nodiscard]] auto Initialize(Vendor::Windows::HINSTANCE hInstance) -> bool;

  // 运行应用程序
  [[nodiscard]] auto Run() -> int;

 private:
  // 应用状态
  Core::State::AppState m_app_state{};
};
