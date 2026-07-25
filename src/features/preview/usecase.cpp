#include "features/preview/usecase.hpp"

#include "vendor/std.hpp"

#include "core/i18n/state.hpp"
#include "core/notifications/notifications.hpp"
#include "core/state/app_state.hpp"
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
    // 启动预览
    // 预览窗与叠加层互斥：以 overlay->enabled（与浮动窗菜单勾选）为准
    if (state.overlay->enabled) {
      state.overlay->enabled = false;
      if (state.overlay->running.load(std::memory_order_acquire)) {
        features::overlay::stop_overlay(state);
      }
      if (state.letterbox->enabled) {
        std::wstring lb_window_title =
            utils::string::FromUtf8(state.settings->raw.window.target_title);
        auto lb_target_window = features::window_control::find_target_window(lb_window_title);
        if (lb_target_window) {
          if (auto lb_result = features::letterbox::show(state, lb_target_window.value());
              !lb_result) {
            Logger().error("Failed to show letterbox: {}", lb_result.error());
          }
        }
      }
      core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                             state.i18n->texts["message.preview_overlay_conflict"]);
    }

    std::wstring window_title = utils::string::FromUtf8(state.settings->raw.window.target_title);
    auto target_window = features::window_control::find_target_window(window_title);

    if (target_window) {
      if (auto result = features::preview::start_preview(state, target_window.value()); !result) {
        Logger().error("Failed to start preview: {}", result.error());
        // 使用新的消息定义并附加错误详情
        std::string error_message =
            state.i18n->texts["message.preview_start_failed"] + result.error();
        core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                               error_message);
      }
    } else {
      Logger().warn("No target window found for preview");
      core::notifications::show_notification(state, state.i18n->texts["label.app_name"],
                                             state.i18n->texts["message.window_not_found"]);
    }
  } else {
    // 停止预览
    features::preview::stop_preview(state);
  }
}

}  // namespace features::preview
