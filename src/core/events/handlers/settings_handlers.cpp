#include "core/events/handlers/settings_handlers.hpp"

#include "vendor/std.hpp"

#include "core/commands/registry.hpp"
#include "core/events/events.hpp"
#include "core/http_server/http_server.hpp"
#include "core/http_server/sse_manager.hpp"
#include "core/i18n/i18n.hpp"
#include "core/rpc/notification_hub.hpp"
#include "core/state/app_state.hpp"
#include "core/webview/webview.hpp"
#include "extensions/infinity_nikki/photo_service.hpp"
#include "extensions/infinity_nikki/task_service.hpp"
#include "features/gallery/gallery.hpp"
#include "features/settings/events.hpp"
#include "features/settings/types.hpp"
#include "ui/floating_window/floating_window.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/photography_panel/photography_panel.hpp"
#include "ui/webview_window/webview_window.hpp"
#include "utils/logger/logger.hpp"

namespace core::events::handlers {

auto has_hotkey_changes(const features::settings::AppSettings& old_settings,
                        const features::settings::AppSettings& new_settings) -> bool {
  const auto& old_hotkey = old_settings.app.hotkey;
  const auto& new_hotkey = new_settings.app.hotkey;

  return old_hotkey.floating_window.modifiers != new_hotkey.floating_window.modifiers ||
         old_hotkey.floating_window.key != new_hotkey.floating_window.key ||
         old_hotkey.screenshot.modifiers != new_hotkey.screenshot.modifiers ||
         old_hotkey.screenshot.key != new_hotkey.screenshot.key ||
         old_hotkey.recording.modifiers != new_hotkey.recording.modifiers ||
         old_hotkey.recording.key != new_hotkey.recording.key;
}

auto refresh_global_hotkeys(core::AppState& state) -> void {
  if (!state.floating_window || !state.floating_window->window.hwnd) {
    Logger().warn("Skip hotkey refresh: floating window handle is not ready");
    return;
  }

  auto hwnd = state.floating_window->window.hwnd;
  core::commands::unregister_all_hotkeys(state, hwnd);
  core::commands::register_all_hotkeys(state, hwnd);
  Logger().info("Global hotkeys refreshed from latest settings");
}

auto has_webview_host_mode_changes(const features::settings::AppSettings& old_settings,
                                   const features::settings::AppSettings& new_settings) -> bool {
  return old_settings.ui.webview_window.enable_transparent_background !=
         new_settings.ui.webview_window.enable_transparent_background;
}

auto has_webview_theme_mode_changes(const features::settings::AppSettings& old_settings,
                                    const features::settings::AppSettings& new_settings) -> bool {
  return old_settings.ui.web_theme.mode != new_settings.ui.web_theme.mode;
}

auto has_language_changes(const features::settings::AppSettings& old_settings,
                          const features::settings::AppSettings& new_settings) -> bool {
  return old_settings.app.language.current != new_settings.app.language.current;
}

auto has_logger_level_changes(const features::settings::AppSettings& old_settings,
                              const features::settings::AppSettings& new_settings) -> bool {
  return old_settings.app.logger.level != new_settings.app.logger.level;
}

auto has_infinity_nikki_hardlink_setting_changes(
    const features::settings::AppSettings& old_settings,
    const features::settings::AppSettings& new_settings) -> bool {
  const auto& old_config = old_settings.extensions.infinity_nikki;
  const auto& new_config = new_settings.extensions.infinity_nikki;

  return old_config.enable != new_config.enable || old_config.game_dir != new_config.game_dir ||
         old_config.gallery_guide_seen != new_config.gallery_guide_seen ||
         old_config.manage_media_hardlinks != new_config.manage_media_hardlinks;
}

auto should_start_infinity_nikki_hardlinks_initialization(
    const features::settings::AppSettings& old_settings,
    const features::settings::AppSettings& new_settings) -> bool {
  const auto& old_config = old_settings.extensions.infinity_nikki;
  const auto& new_config = new_settings.extensions.infinity_nikki;

  if (!new_config.enable || new_config.game_dir.empty() || !new_config.gallery_guide_seen ||
      !new_config.manage_media_hardlinks) {
    return false;
  }

  return (!old_config.enable && new_config.enable) || old_config.game_dir != new_config.game_dir ||
         (!old_config.gallery_guide_seen && new_config.gallery_guide_seen) ||
         (!old_config.manage_media_hardlinks && new_config.manage_media_hardlinks);
}

auto apply_runtime_language_from_settings(core::AppState& state,
                                          const features::settings::AppSettings& settings) -> void {
  if (!state.i18n) {
    Logger().warn("Skip runtime language sync: i18n state is not ready");
    return;
  }

  const auto& locale = settings.app.language.current;
  if (auto result = core::i18n::load_language_by_locale(state, locale); !result) {
    Logger().warn("Failed to apply runtime language ('{}'): {}", locale, result.error());
    return;
  }

  Logger().info("Runtime language switched to {}", locale);
}

auto apply_runtime_logger_level_from_settings(core::AppState& state,
                                              const features::settings::AppSettings& settings)
    -> void {
  const auto& level = settings.app.logger.level;
  if (auto result = utils::logging::set_level(level); !result) {
    Logger().warn("Failed to apply runtime logger level ('{}'): {}", level, result.error());
    return;
  }

  Logger().debug("Runtime logger level switched to {}", level);
}

// 处理设置变更事件
auto handle_settings_changed(core::AppState& state,
                             const features::settings::events::SettingsChangeEvent& event) -> void {
  try {
    Logger().info("Settings changed: {}", event.data.change_description);

    if (has_language_changes(event.data.old_settings, event.data.new_settings)) {
      apply_runtime_language_from_settings(state, event.data.new_settings);
    }

    if (has_logger_level_changes(event.data.old_settings, event.data.new_settings)) {
      apply_runtime_logger_level_from_settings(state, event.data.new_settings);
    }

    // 原生浮层统一走同一轮刷新
    ui::floating_window::refresh_from_settings(state);
    ui::photography_panel::refresh_from_settings(state);

    if (!event.data.old_settings.app.onboarding.completed &&
        event.data.new_settings.app.onboarding.completed) {
      Logger().info("Onboarding completed, showing floating window and closing webview");
      features::gallery::ensure_output_directory_media_source(
          state, event.data.new_settings.features.output_dir_path);
      ui::floating_window::show_window(state);
      auto _ = ui::webview_window::close_window(state);
    }

    if (has_hotkey_changes(event.data.old_settings, event.data.new_settings)) {
      refresh_global_hotkeys(state);
    }

    if (has_infinity_nikki_hardlink_setting_changes(event.data.old_settings,
                                                    event.data.new_settings)) {
      extensions::infinity_nikki::photo_service::refresh_from_settings(state);

      if (should_start_infinity_nikki_hardlinks_initialization(event.data.old_settings,
                                                               event.data.new_settings)) {
        auto task_result =
            extensions::infinity_nikki::task_service::start_initialize_media_hardlinks_task(state);
        if (!task_result) {
          Logger().warn("Failed to start Infinity Nikki media hardlink task: {}",
                        task_result.error());
        } else {
          Logger().info("Infinity Nikki media hardlink task started: {}", task_result.value());
        }
      }
    }

    auto webview_host_mode_changed =
        has_webview_host_mode_changes(event.data.old_settings, event.data.new_settings);
    auto webview_theme_mode_changed =
        has_webview_theme_mode_changes(event.data.old_settings, event.data.new_settings);

    const bool old_lan_enabled = event.data.old_settings.app.lan_access.enabled;
    const bool new_lan_enabled = event.data.new_settings.app.lan_access.enabled;

    if (old_lan_enabled != new_lan_enabled) {
      // 关闭 LAN 后立即踢出远端 SSE，避免连接继续接收旧会话的推送。
      if (!new_lan_enabled) {
        core::http_server::sse_manager::request_close_all_connections(state);
      }

      if (auto rebind_result = core::http_server::rebind_listen_socket(state, new_lan_enabled);
          !rebind_result) {
        Logger().warn("Failed to apply LAN access listener change: {}", rebind_result.error());
      }
    }

    if (webview_host_mode_changed) {
      if (auto recreate_result = ui::webview_window::recreate_webview_host(state);
          !recreate_result) {
        Logger().warn("Failed to recreate WebView host after settings change: {}",
                      recreate_result.error());
      }
    } else if (webview_theme_mode_changed) {
      core::webview::apply_background_mode_from_settings(state);
    }

    core::rpc::notification_hub::send_notification(state, "settings.changed");

    Logger().debug("Settings change processing completed");

  } catch (const std::exception& e) {
    Logger().error("Error handling settings change event: {}", e.what());
  }
}

auto register_settings_handlers(core::AppState& app_state) -> void {
  using namespace core::events;

  // 注册设置变更事件处理器
  subscribe<features::settings::events::SettingsChangeEvent>(
      app_state, [&app_state](const features::settings::events::SettingsChangeEvent& event) {
        handle_settings_changed(app_state, event);
      });
}

}  // namespace core::events::handlers
