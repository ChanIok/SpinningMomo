module;

module Handlers.UI;

import std;
import Core.Events;
import Core.State;
import UI.AppWindow;
import Utils.Logger;

namespace Handlers {

auto register_ui_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册比例改变事件处理器
  subscribe(app_state.event_bus, EventType::RatioChanged, [&app_state](const Event& event) {
    auto data = std::any_cast<RatioChangeData>(event.data);
    Logger().debug("Ratio changed to index {}", data.index);
    UI::AppWindow::set_current_ratio(app_state, data.index);
  });

  // 注册分辨率改变事件处理器
  subscribe(app_state.event_bus, EventType::ResolutionChanged, [&app_state](const Event& event) {
    auto data = std::any_cast<ResolutionChangeData>(event.data);
    Logger().debug("Resolution changed to index {}", data.index);
    UI::AppWindow::set_current_resolution(app_state, data.index);
    // 注意：分辨率选择也可能是临时状态
  });
}

}  // namespace Handlers