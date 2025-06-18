module;

module Handlers.System;

import std;
import Core.Events;
import Core.State;
import UI.AppWindow;
import Utils.Logger;
import Vendor.Windows;

namespace Handlers {

auto register_system_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册系统命令事件处理器
  subscribe(app_state.event_bus, EventType::SystemCommand, [&app_state](const Event& event) {
    auto command = std::any_cast<std::string>(event.data);
    Logger().debug("System command received");

    if (command == "toggle_visibility") {
      UI::AppWindow::toggle_visibility(app_state);
    }
  });

  // 注册窗口动作事件处理器
  subscribe(app_state.event_bus, EventType::WindowAction, [&app_state](const Event& event) {
    auto action = std::any_cast<WindowAction>(event.data);
    Logger().debug("Window action triggered");

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
        UI::AppWindow::hide_window(app_state);
        break;
      case WindowAction::Exit:
        UI::AppWindow::destroy_window(app_state);
        break;
    }
  });
}

} 