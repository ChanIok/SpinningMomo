#include "features/preview/usecase.hpp"

#include "vendor/std.hpp"

#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/state/app_state.hpp"
#include "features/adb_mode/device_session.hpp"
#include "features/letterbox/letterbox.hpp"
#include "features/letterbox/state.hpp"
#include "features/overlay/overlay.hpp"
#include "features/overlay/state.hpp"
#include "features/preview/preview.hpp"
#include "features/preview/state.hpp"
#include "features/settings/state.hpp"
#include "features/window_control/window_control.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::preview {

// 切换预览功能
auto toggle_preview(core::AppState& state) -> void {
  bool is_running = state.preview && state.preview->running.load(std::memory_order_acquire);

  if (!is_running) {
    if (features::adb_mode::session::is_connected(state)) {
      core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                             state.i18n->texts["message.adb_feature_unavailable"]);
      return;
    }

    std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
    auto target_window = features::window_control::find_target_window(window_title);
    if (!target_window) {
      core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                             state.i18n->texts["message.window_not_found"]);
      return;
    }
    auto monitor_info = features::window_control::resolve_working_monitor(state);
    if (!monitor_info) {
      core::notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.preview_start_failed"] + monitor_info.error());
      return;
    }

    // 启动预览
    // 预览窗与叠加层互斥：以 overlay->enabled（与浮动窗菜单勾选）为准
    if (state.overlay->enabled) {
      state.overlay->enabled = false;
      if (state.overlay->running.load(std::memory_order_acquire)) {
        features::overlay::stop_overlay(state);
      }
      // 从叠加层切换到预览时，恢复独立黑边窗口。
      if (state.letterbox->enabled) {
        if (auto result = features::letterbox::show(state, *target_window, *monitor_info);
            !result) {
          Logger().error("Failed to restore letterbox after disabling overlay: {}", result.error());
        }
      }
      core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                             state.i18n->texts["message.preview_overlay_conflict"]);
    }

    if (auto result = features::preview::start_preview(state, *target_window, *monitor_info);
        !result) {
      Logger().error("Failed to start preview: {}", result.error());
      core::notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.preview_start_failed"] + result.error());
    }
  } else {
    // 停止预览
    features::preview::stop_preview(state);
  }
}

}  // namespace features::preview
