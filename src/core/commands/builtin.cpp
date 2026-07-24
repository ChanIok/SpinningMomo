

#include "core/commands/registry.hpp"
#include "core/commands/state.hpp"
#include "core/commands/types.hpp"
#include "core/state/app_state.hpp"
#include "features/letterbox/state.hpp"
#include "features/letterbox/usecase.hpp"
#include "features/overlay/state.hpp"
#include "features/overlay/usecase.hpp"
#include "features/photography/state.hpp"
#include "features/photography/usecase.hpp"
#include "features/preview/state.hpp"
#include "features/preview/usecase.hpp"
#include "features/recording/state.hpp"
#include "features/recording/usecase.hpp"
#include "features/screenshot/usecase.hpp"
#include "features/settings/state.hpp"
#include "features/window_control/usecase.hpp"
#include "ui/floating_window/floating_window.hpp"
#include "ui/webview_window/webview_window.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"
#include "utils/system/system.hpp"
#include "vendor/windows.hpp"

namespace Core::Commands {

// 注册单个内置命令：拒绝重复 ID，并把行为所有权移入注册表
auto register_command(CommandRegistry& registry, CommandDescriptor descriptor) -> void {
  const std::string id = descriptor.id;

  if (registry.descriptors.contains(id)) {
    Logger().warn("Command already registered: {}", id);
    return;
  }

  // 注册表独占 action/get_state，registration_order 只保存稳定 ID
  registry.descriptors.emplace(id, std::move(descriptor));
  registry.registration_order.push_back(id);

  Logger().debug("Registered command: {}", id);
}

// 注册所有内置命令
auto register_builtin_commands(Core::State::AppState& state) -> void {
  auto& registry = state.commands->registry;
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
  register_command(
      registry, {
                    .id = "preview.toggle",
                    .i18n_key = "menu.preview_toggle",
                    .is_toggle = true,
                    .action =
                        [&state]() {
                          Features::Preview::UseCase::toggle_preview(state);
                          UI::FloatingWindow::request_repaint(state);
                        },
                    .get_state = [&state]() -> bool {
                      return state.preview ? state.preview->running.load(std::memory_order_acquire)
                                           : false;
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

  // 切换高级摄影
  register_command(registry,
                   {
                       .id = "photography.toggle",
                       .i18n_key = "menu.photography_toggle",
                       .is_toggle = true,
                       .action =
                           [&state]() {
                             Features::Photography::UseCase::toggle(state);
                             UI::FloatingWindow::request_repaint(state);
                           },
                       .get_state = [&state]() -> bool { return state.photography->enabled; },
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
      registry, {
                    .id = "recording.toggle",
                    .i18n_key = "menu.recording_toggle",
                    .is_toggle = true,
                    .action =
                        [&state]() {
                          if (auto result = Features::Recording::UseCase::toggle_recording(state);
                              !result) {
                            Logger().error("Recording toggle failed: {}", result.error());
                          }
                          UI::FloatingWindow::request_repaint(state);
                        },
                    .get_state = [&state]() -> bool {
                      if (!state.recording) {
                        return false;
                      }

                      const auto status = state.recording->status.load(std::memory_order_acquire);
                      return status == Features::Recording::Types::RecordingStatus::Starting ||
                             status == Features::Recording::Types::RecordingStatus::Recording ||
                             status == Features::Recording::Types::RecordingStatus::Stopping;
                    },
                    .hotkey =
                        HotkeyBinding{
                            .modifiers = 0,  // 无修饰键
                            .key = 0x77,     // VK_F8 (F8)
                            .settings_path = "app.hotkey.recording",
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
