module;

module Core.Actions;

import std;
import Core.State;
import Utils.Logger;
import Vendor.Windows;

namespace Core::Actions {

struct ActionHandler {
  Core::State::AppState& state;

  auto operator()(const Payloads::SetCurrentRatio& payload) const -> void {
    if (payload.index < state.app_window.data.ratios.size() ||
        payload.index == std::numeric_limits<size_t>::max()) {
      state.app_window.ui.current_ratio_index = payload.index;
      Logger().debug("Ratio updated to index: {}", payload.index);
    } else {
      Logger().warn("Invalid ratio index: {}", payload.index);
    }
  }

  auto operator()(const Payloads::SetCurrentResolution& payload) const -> void {
    if (payload.index < state.app_window.data.resolutions.size()) {
      state.app_window.ui.current_resolution_index = payload.index;
      Logger().debug("Resolution updated to index: {}", payload.index);
    } else {
      Logger().warn("Invalid resolution index: {}", payload.index);
    }
  }

  auto operator()(const Payloads::TogglePreview& payload) const -> void {
    state.app_window.ui.preview_enabled = payload.enabled;
    Logger().debug("Preview toggled to: {}", payload.enabled);
  }

  auto operator()(const Payloads::ToggleOverlay& payload) const -> void {
    state.app_window.ui.overlay_enabled = payload.enabled;
    Logger().debug("Overlay toggled to: {}", payload.enabled);
  }

  auto operator()(const Payloads::ToggleLetterbox& payload) const -> void {
    state.app_window.ui.letterbox_enabled = payload.enabled;
    Logger().debug("Letterbox toggled to: {}", payload.enabled);
  }

  auto operator()(const Payloads::ResetWindowState& /*payload*/) const -> void {
    // 重置UI状态到默认值
    state.app_window.ui.current_ratio_index =
        std::numeric_limits<size_t>::max();  // 表示使用屏幕比例
    state.app_window.ui.current_resolution_index = 0;
    Logger().debug("Window state reset");
  }

  auto operator()(const Payloads::UpdateHoverIndex& payload) const -> void {
    state.app_window.ui.hover_index = payload.index;
    // Logger().debug("Hover index updated to: {}", payload.index);
  }
};

auto dispatch_action(Core::State::AppState& state, const Action& action) -> void {
  // Logger().debug("Dispatching action with variant index: {}", action.payload.index());
  std::visit(ActionHandler{state}, action.payload);
  // Logger().debug("Action processed successfully");
}

auto trigger_ui_update(Core::State::AppState& state) -> void {
  if (state.app_window.window.hwnd) {
    Vendor::Windows::InvalidateRect(state.app_window.window.hwnd, nullptr, true);
  }
}

}  // namespace Core::Actions