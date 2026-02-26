module;

module Core.Commands;

import std;
import Core.State;
import Core.Commands;
import Features.Settings.State;
import Features.Screenshot.UseCase;
import Features.Recording.UseCase;
import Features.ReplayBuffer.UseCase;
import Features.ReplayBuffer.Types;
import Features.ReplayBuffer.State;
import Features.Letterbox.UseCase;
import Features.Overlay.UseCase;
import Features.Preview.UseCase;
import Features.Letterbox.State;
import Features.Recording.State;
import Features.Overlay.State;
import Features.Preview.State;
import Features.VirtualGamepad;
import Features.VirtualGamepad.State;
import Features.WindowControl.UseCase;
import UI.FloatingWindow;
import UI.WebViewWindow;
import Utils.Logger;
import Utils.Path;
import Utils.System;
import Vendor.BuildConfig;
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
                       .action = [&state]() { UI::WebViewWindow::activate_window(state); },
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
                       .hotkey =
                           HotkeyBinding{
                               .modifiers = 1,  // MOD_CONTROL
                               .key = 192,      // VK_OEM_3 (`)
                               .settings_path = "app.hotkey.floating_window",
                           },
                   });

  // === 截图功能 ===

  // 截图
  register_command(registry,
                   {
                       .id = "screenshot.capture",
                       .i18n_key = "menu.screenshot_capture",
                       .is_toggle = false,
                       .action = [&state]() { Features::Screenshot::UseCase::capture(state); },
                       .hotkey =
                           HotkeyBinding{
                               .modifiers = 0,  // 无修饰键
                               .key = 44,       // VK_SNAPSHOT (PrintScreen)
                               .settings_path = "app.hotkey.screenshot",
                           },
                   });

  // 打开输出目录
  register_command(
      registry,
      {
          .id = "output.open_folder",
          .i18n_key = "menu.output_open_folder",
          .is_toggle = false,
          .action =
              [&state]() {
                auto output_dir_result =
                    Utils::Path::GetOutputDirectory(state.settings->raw.features.output_dir_path);
                if (!output_dir_result) {
                  Logger().error("Failed to resolve output directory: {}",
                                 output_dir_result.error());
                  return;
                }

                auto open_result = Utils::System::open_directory(output_dir_result.value());
                if (!open_result) {
                  Logger().error("Failed to open output directory: {}", open_result.error());
                }
              },
      });

  // 打开游戏相册目录
  register_command(
      registry,
      {
          .id = "external_album.open_folder",
          .i18n_key = "menu.external_album_open_folder",
          .is_toggle = false,
          .action =
              [&state]() {
                std::filesystem::path folder_to_open;

                const auto& external_album_path = state.settings->raw.features.external_album_path;
                if (!external_album_path.empty()) {
                  folder_to_open = external_album_path;
                } else {
                  auto output_dir_result =
                      Utils::Path::GetOutputDirectory(state.settings->raw.features.output_dir_path);
                  if (!output_dir_result) {
                    Logger().error("Failed to resolve fallback output directory: {}",
                                   output_dir_result.error());
                    return;
                  }
                  folder_to_open = output_dir_result.value();
                }

                auto open_result = Utils::System::open_directory(folder_to_open);
                if (!open_result) {
                  Logger().error("Failed to open external album directory: {}",
                                 open_result.error());
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
          .hotkey =
              HotkeyBinding{
                  .modifiers = 0,  // 无修饰键
                  .key = 0x77,     // VK_F8 (F8)
                  .settings_path = "app.hotkey.recording",
              },
      });

  // === 动态照片和即时回放 ===
  if (Vendor::BuildConfig::is_debug_build()) {
    // 切换动态照片模式（仅运行时）
    register_command(
        registry,
        {
            .id = "motion_photo.toggle",
            .i18n_key = "menu.motion_photo_toggle",
            .is_toggle = true,
            .action =
                [&state]() {
                  if (auto result = Features::ReplayBuffer::UseCase::toggle_motion_photo(state);
                      !result) {
                    Logger().error("Motion Photo toggle failed: {}", result.error());
                  }
                  UI::FloatingWindow::request_repaint(state);
                },
            .get_state = [&state]() -> bool {
              return state.replay_buffer &&
                     state.replay_buffer->motion_photo_enabled.load(std::memory_order_acquire);
            },
        });

    // 切换即时回放模式（仅运行时）
    register_command(
        registry,
        {
            .id = "replay_buffer.toggle",
            .i18n_key = "menu.replay_buffer_toggle",
            .is_toggle = true,
            .action =
                [&state]() {
                  if (auto result = Features::ReplayBuffer::UseCase::toggle_replay_buffer(state);
                      !result) {
                    Logger().error("Instant Replay toggle failed: {}", result.error());
                  }
                  UI::FloatingWindow::request_repaint(state);
                },
            .get_state = [&state]() -> bool {
              return state.replay_buffer &&
                     state.replay_buffer->replay_enabled.load(std::memory_order_acquire);
            },
        });

    // 保存即时回放
    register_command(
        registry, {
                      .id = "replay_buffer.save",
                      .i18n_key = "menu.replay_buffer_save",
                      .is_toggle = false,
                      .action =
                          [&state]() {
                            if (auto result = Features::ReplayBuffer::UseCase::save_replay(state);
                                !result) {
                              Logger().error("Save replay failed: {}", result.error());
                            }
                          },
                  });
  } else {
    Logger().info("Skipping experimental replay commands in release build");
  }

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

  // === 虚拟手柄 ===
  if (Vendor::BuildConfig::is_debug_build()) {
    // 切换虚拟手柄
    register_command(
        registry,
        {
            .id = "virtual_gamepad.toggle",
            .i18n_key = "menu.virtual_gamepad_toggle",
            .is_toggle = true,
            .action =
                [&state]() {
                  if (auto result = Features::VirtualGamepad::toggle(state); !result) {
                    Logger().error("Virtual gamepad toggle failed: {}", result.error());
                  }
                  UI::FloatingWindow::request_repaint(state);
                },
            .get_state = [&state]() -> bool { return Features::VirtualGamepad::is_enabled(state); },
        });
  } else {
    Logger().info("Skipping experimental virtual gamepad command in release build");
  }

  Logger().info("Registered {} builtin commands", registry.descriptors.size());
}

}  // namespace Core::Commands
