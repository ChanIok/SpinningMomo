module;

module Core.Commands;

import std;
import Core.State;
import Core.Commands;
import Features.Settings.State;
import Features.Screenshot.UseCase;
import Features.Screenshot.Folder;
import Features.Recording.UseCase;
import Features.Letterbox.UseCase;
import Features.Overlay.UseCase;
import Features.Preview.UseCase;
import Features.Letterbox.State;
import Features.Recording.State;
import Features.Overlay.State;
import Features.Preview.State;
import Features.WindowControl.UseCase;
import UI.FloatingWindow;
import UI.WebViewWindow;
import Utils.Logger;
import Vendor.Windows;

namespace Core::Commands {

// 注册所有内置命令
auto register_builtin_commands(Core::State::AppState& state, CommandRegistry& registry) -> void {
  Logger().info("Registering builtin commands...");

  // === 应用层命令 ===

  // 打开主界面（WebView2 或浏览器）
  register_command(registry,
                   {
                       .id = "app.main",
                       .i18n_key = "menu.app_main",
                       .is_toggle = false,
                       .action = [&state]() { UI::WebViewWindow::toggle_visibility(state); },
                   });

  // 退出应用
  register_command(registry, {
                                 .id = "app.exit",
                                 .i18n_key = "menu.app_exit",
                                 .is_toggle = false,
                                 .action = []() { Vendor::Windows::PostQuitMessage(0); },
                             });

  // === 悬浮窗控制 ===

  // 激活悬浮窗
  register_command(registry,
                   {
                       .id = "app.float",
                       .i18n_key = "menu.app_float",
                       .is_toggle = false,
                       .action = [&state]() { UI::FloatingWindow::toggle_visibility(state); },
                   });

  // === 截图功能 ===

  // 截图
  register_command(registry,
                   {
                       .id = "screenshot.capture",
                       .i18n_key = "menu.screenshot_capture",
                       .is_toggle = false,
                       .action = [&state]() { Features::Screenshot::UseCase::capture(state); },
                   });

  // 打开截图文件夹
  register_command(
      registry, {
                    .id = "screenshot.open_folder",
                    .i18n_key = "menu.screenshot_open_folder",
                    .is_toggle = false,
                    .action =
                        [&state]() {
                          auto result = Features::Screenshot::Folder::open_folder(state);
                          if (!result) {
                            Logger().error("Failed to open screenshot folder: {}", result.error());
                          }
                        },
                });

  // === 独立功能 ===

  // 切换预览窗
  register_command(registry, {
                                 .id = "preview.toggle",
                                 .i18n_key = "menu.preview_toggle",
                                 .is_toggle = true,
                                 .action =
                                     [&state]() {
                                       Features::Preview::UseCase::toggle_preview(state);
                                       UI::FloatingWindow::request_repaint(state);
                                     },
                                 .get_state = [&state]() -> bool {
                                   return state.preview ? state.preview->running : false;
                                 },
                             });

  // 切换叠加层
  register_command(registry, {
                                 .id = "overlay.toggle",
                                 .i18n_key = "menu.overlay_toggle",
                                 .is_toggle = true,
                                 .action =
                                     [&state]() {
                                       Features::Overlay::UseCase::toggle_overlay(state);
                                       UI::FloatingWindow::request_repaint(state);
                                     },
                                 .get_state = [&state]() -> bool {
                                   return state.overlay && state.overlay->enabled;
                                 },
                             });

  // 切换黑边模式
  register_command(registry, {
                                 .id = "letterbox.toggle",
                                 .i18n_key = "menu.letterbox_toggle",
                                 .is_toggle = true,
                                 .action =
                                     [&state]() {
                                       Features::Letterbox::UseCase::toggle_letterbox(state);
                                       UI::FloatingWindow::request_repaint(state);
                                     },
                                 .get_state = [&state]() -> bool {
                                   return state.letterbox && state.letterbox->enabled;
                                 },
                             });

  // 切换录制
  register_command(
      registry,
      {
          .id = "recording.toggle",
          .i18n_key = "menu.recording_toggle",
          .is_toggle = true,
          .action =
              [&state]() {
                if (auto result = Features::Recording::UseCase::toggle_recording(state); !result) {
                  Logger().error("Recording toggle failed: {}", result.error());
                }
                UI::FloatingWindow::request_repaint(state);
              },
          .get_state = [&state]() -> bool {
            return state.recording && state.recording->status ==
                                          Features::Recording::Types::RecordingStatus::Recording;
          },
      });

  // === 窗口操作 ===

  // 重置窗口变换
  register_command(
      registry,
      {
          .id = "window.reset",
          .i18n_key = "menu.window_reset",
          .is_toggle = false,
          .action = [&state]() { Features::WindowControl::UseCase::reset_window_transform(state); },
      });

  Logger().info("Registered {} builtin commands", registry.descriptors.size());
}

}  // namespace Core::Commands
