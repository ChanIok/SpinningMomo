module;

module Features.Registry;

import std;
import Core.State;
import Features.Registry;
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
import UI.AppWindow;
import Utils.Logger;

namespace Features::Registry {

// 注册所有内置功能
auto register_builtin_features(Core::State::AppState& state, FeatureRegistry& registry) -> void {
  Logger().info("Registering builtin features...");

  // === Screenshot 分组 ===

  // 截图
  register_feature(registry,
                   {
                       .id = "screenshot.capture",
                       .i18n_key = "menu.screenshot_capture",
                       .is_toggle = false,
                       .action = [&state]() { Features::Screenshot::UseCase::capture(state); },
                   });

  // 打开截图文件夹
  register_feature(
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

  // === Feature 分组 ===

  // 切换预览窗口
  register_feature(registry, {
                                 .id = "feature.toggle_preview",
                                 .i18n_key = "menu.preview_toggle",
                                 .is_toggle = true,
                                 .action =
                                     [&state]() {
                                       Features::Preview::UseCase::toggle_preview(state);
                                       UI::AppWindow::request_repaint(state);
                                     },
                                 .get_state = [&state]() -> bool {
                                   return state.preview ? state.preview->running : false;
                                 },
                             });

  // 切换叠加层
  register_feature(registry, {
                                 .id = "feature.toggle_overlay",
                                 .i18n_key = "menu.overlay_toggle",
                                 .is_toggle = true,
                                 .action =
                                     [&state]() {
                                       Features::Overlay::UseCase::toggle_overlay(state);
                                       UI::AppWindow::request_repaint(state);
                                     },
                                 .get_state = [&state]() -> bool {
                                   return state.overlay ? state.overlay->running : false;
                                 },
                             });

  // 切换黑边模式
  register_feature(registry, {
                                 .id = "feature.toggle_letterbox",
                                 .i18n_key = "menu.letterbox_toggle",
                                 .is_toggle = true,
                                 .action =
                                     [&state]() {
                                       Features::Letterbox::UseCase::toggle_letterbox(state);
                                       UI::AppWindow::request_repaint(state);
                                     },
                                 .get_state = [&state]() -> bool {
                                   return state.letterbox && state.letterbox->window_handle &&
                                          IsWindowVisible(state.letterbox->window_handle);
                                 },
                             });

  // 切换录制
  register_feature(
      registry,
      {
          .id = "feature.toggle_recording",
          .i18n_key = "menu.recording_toggle",
          .is_toggle = true,
          .action =
              [&state]() {
                if (auto result = Features::Recording::UseCase::toggle_recording(state); !result) {
                  Logger().error("Recording toggle failed: {}", result.error());
                }
                UI::AppWindow::request_repaint(state);
              },
          .get_state = [&state]() -> bool {
            return state.recording && state.recording->status ==
                                          Features::Recording::Types::RecordingStatus::Recording;
          },
      });

  // === Window 分组 ===

  // 重置窗口变换
  register_feature(
      registry,
      {
          .id = "window.reset_transform",
          .i18n_key = "menu.window_reset",
          .is_toggle = false,
          .action = [&state]() { Features::WindowControl::UseCase::reset_window_transform(state); },
      });

  // === Panel 分组 ===

  // 隐藏面板
  register_feature(registry, {
                                 .id = "panel.hide",
                                 .i18n_key = "menu.app_hide",
                                 .is_toggle = false,
                                 .action = [&state]() { UI::AppWindow::hide_window(state); },
                             });

  // === App 分组 ===

  // 退出应用
  register_feature(registry, {
                                 .id = "app.exit",
                                 .i18n_key = "menu.app_exit",
                                 .is_toggle = false,
                                 .action = []() { ::PostQuitMessage(0); },
                             });

  Logger().info("Registered {} builtin features", registry.descriptors.size());
}

}  // namespace Features::Registry
