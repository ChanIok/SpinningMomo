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
}

} 