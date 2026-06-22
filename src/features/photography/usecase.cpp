module;

module Features.Photography.UseCase;

import std;
import Core.I18n.State;
import Core.Notifications;
import Core.State;
import Features.Photography.State;
import UI.PhotographyPanel;
import Utils.Logger;

namespace Features::Photography::UseCase {

// 显示面板并标记高级摄影启用
auto start(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto show_result = UI::PhotographyPanel::show(state);
  if (!show_result) {
    return std::unexpected(show_result.error());
  }

  const bool previous = state.photography->enabled.exchange(true, std::memory_order_acq_rel);
  if (!previous) {
    Logger().info("Photography mode started");
  }
  return {};
}

// 停止高级摄影时关闭面板
auto stop(Core::State::AppState& state) -> void {
  const bool previous = state.photography->enabled.exchange(false, std::memory_order_acq_rel);
  UI::PhotographyPanel::hide(state);

  if (previous) {
    Logger().info("Photography mode stopped");
  }
}

// 已启用则停止；否则尝试启动，失败时弹通知。
auto toggle(Core::State::AppState& state) -> void {
  if (state.photography->enabled.load(std::memory_order_acquire)) {
    stop(state);
    return;
  }

  if (auto result = start(state); !result) {
    Logger().error("Failed to start photography mode: {}", result.error());
    Core::Notifications::show_notification(
        state, state.i18n->texts["label.app_name"],
        state.i18n->texts["message.photography_start_failed"] + result.error());
  }
}

auto cleanup(Core::State::AppState& state) -> void {
  state.photography->enabled.store(false, std::memory_order_release);
}

auto handle_panel_close(Core::State::AppState& state) -> void {
  state.photography->enabled.store(false, std::memory_order_release);
}

}  // namespace Features::Photography::UseCase
