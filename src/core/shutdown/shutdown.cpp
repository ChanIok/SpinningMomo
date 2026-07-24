#include "core/shutdown/shutdown.hpp"

#include "vendor/std.hpp"

#include "core/async/async.hpp"
#include "core/commands/registry.hpp"
#include "core/database/database.hpp"
#include "core/dialog_service/dialog_service.hpp"
#include "core/http_client/http_client.hpp"
#include "core/http_server/http_server.hpp"
#include "core/state/app_state.hpp"
#include "core/worker_pool/worker_pool.hpp"
#include "extensions/infinity_nikki/photo_service.hpp"
#include "features/gallery/gallery.hpp"
#include "features/letterbox/letterbox.hpp"
#include "features/overlay/overlay.hpp"
#include "features/photography/usecase.hpp"
#include "features/preview/preview.hpp"
#include "features/recording/usecase.hpp"
#include "features/screenshot/screenshot.hpp"
#include "features/update/state.hpp"
#include "features/update/update.hpp"
#include "features/window_control/window_control.hpp"
#include "ui/context_menu/context_menu.hpp"
#include "ui/floating_window/floating_window.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/notification_window/notification_window.hpp"
#include "ui/photography_panel/photography_panel.hpp"
#include "ui/tray_icon/tray_icon.hpp"
#include "ui/webview_window/webview_window.hpp"
#include "utils/logger/logger.hpp"

namespace core::shutdown {

// 按初始化的反向依赖关闭应用，先收敛后台任务再释放 UI 和核心服务。
auto shutdown_application(core::AppState& state) -> void {
  Logger().info("==================================================");
  Logger().info("SpinningMomo shutdown begin");
  Logger().info("==================================================");

  // 先卸载键盘、鼠标钩子，避免后续清理过程输入事件被拦截
  core::commands::uninstall_keyboard_keepalive_hook(state);

  core::commands::unregister_all_hotkeys(state, state.floating_window->window.hwnd);

  features::window_control::stop_center_lock_monitor(state);

  // 清理顺序应该与 core::initializer::initialize_application 中的初始化顺序相反
  // 先停止录制并等待录制切换线程结束，避免与后续 UI/核心清理并发
  features::recording::stop_recording_if_running(state);

  core::dialog_service::stop(state);

  auto shutdown_gallery_extensions = [](core::AppState& app_state) {
    extensions::infinity_nikki::photo_service::shutdown(app_state);
  };
  features::gallery::cleanup(state, std::move(shutdown_gallery_extensions));

  // 1. UI 清理
  ui::context_menu::cleanup(state);
  ui::tray_icon::destroy(state);
  ui::notification_window::cleanup(state);
  ui::photography_panel::cleanup(state);
  ui::floating_window::destroy_window(state);
  ui::webview_window::cleanup(state);

  // 2. 功能模块清理
  // 检查是否有待处理的更新
  if (state.update->pending_update) {
    Logger().info("Executing pending update on program exit");
    features::update::execute_pending_update(state);
  }
  features::preview::stop_preview(state);
  features::preview::cleanup_preview(state);
  features::photography::stop(state);
  features::photography::cleanup(state);
  features::overlay::stop_overlay(state);
  features::overlay::cleanup_overlay(state);
  if (auto result = features::letterbox::shutdown(state); !result) {
    Logger().error("Failed to shutdown Letterbox: {}", result.error());
  }
  features::screenshot::cleanup_system(state);
  // 3. 核心服务清理
  core::http_server::shutdown(state);
  core::http_client::shutdown(state);

  // 停止工作线程池（等待所有任务完成）
  core::worker_pool::stop(state);

  core::database::shutdown(state);

  core::async::stop(state);

  Logger().info("==================================================");
  Logger().info("SpinningMomo shutdown complete");
  Logger().info("==================================================");
}

}  // namespace core::shutdown
