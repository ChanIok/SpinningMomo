module;

export module App;

import std;
import Vendor.Windows;
import Core.Events;
import Core.State;
import UI.AppWindow;

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

  // 现代C++风格的初始化
  [[nodiscard]] auto Initialize(Vendor::Windows::HINSTANCE hInstance) -> bool;

  // 运行应用程序
  [[nodiscard]] auto Run() -> int;

 private:
  // 应用状态
  std::unique_ptr<Core::State::AppState> m_app_state;

  Vendor::Windows::HINSTANCE m_h_instance = nullptr;

  // 内部辅助函数
  auto LogSystemInfo() -> void;
};