#include "features/overlay/usecase.hpp"

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

namespace features::overlay {

// 切换叠加层功能
auto toggle_overlay(core::AppState& state) -> void {
  bool is_enabled = state.overlay->enabled;

  if (!is_enabled && features::adb_mode::session::is_connected(state)) {
    core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                           state.i18n->texts["message.adb_feature_unavailable"]);
    return;
  }

  if (!is_enabled) {
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
          state.i18n->texts["message.overlay_start_failed"] + monitor_info.error());
      return;
    }
    state.overlay->enabled = true;

    // 用户想启用叠加层
    // 预览窗与叠加层互斥，若预览窗运行则先关闭
    if (state.preview && state.preview->running.load(std::memory_order_acquire)) {
      features::preview::stop_preview(state);
      core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                             state.i18n->texts["message.preview_overlay_conflict"]);
    }
    // 如果启用了黑边模式，关闭黑边窗口
    if (state.letterbox->enabled) {
      if (auto result = features::letterbox::shutdown(state); !result) {
        Logger().error("Failed to shutdown letterbox: {}", result.error());
      }
    }

    if (auto result = features::overlay::start_overlay(state, *target_window, *monitor_info);
        !result) {
      Logger().error("Failed to start overlay: {}", result.error());
      state.overlay->enabled = false;
      core::notifications::show_notification(
          state, state.i18n->texts["label.app_name"],
          state.i18n->texts["message.overlay_start_failed"] + result.error());
    }
  } else {
    // 停止不依赖当前屏幕设置，显示器断开后仍可关闭叠加层。
    state.overlay->enabled = false;
    features::overlay::stop_overlay(state);

    // 如果启用了黑边模式，重新显示黑边窗口
    if (state.letterbox->enabled) {
      std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
      auto target_window = features::window_control::find_target_window(window_title);
      if (target_window) {
        auto monitor_info = features::window_control::resolve_working_monitor(state);
        if (!monitor_info) {
          core::notifications::show_notification(
              state, state.i18n->texts["label.app_name"],
              state.i18n->texts["message.overlay_start_failed"] + monitor_info.error());
          return;
        }
        if (auto result = features::letterbox::show(state, *target_window, *monitor_info);
            !result) {
          Logger().error("Failed to show letterbox: {}", result.error());
        }
      }
    }
  }
}

}  // namespace features::overlay
