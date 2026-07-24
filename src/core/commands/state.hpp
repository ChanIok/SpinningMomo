#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/commands/types.hpp"

namespace core::commands {

struct CommandState {
  core::commands::CommandRegistry registry;

  // 常驻低级键盘钩子。
  // 它不负责业务按键处理，只用于维持进程级全局输入挂载。
  HHOOK keyboard_keepalive_hook = nullptr;

  // 热键运行时状态
  std::unordered_map<int, std::string> hotkey_to_command;  // hotkey_id -> command_id
  int next_hotkey_id = 1;                                  // 下一个可用的热键ID

  // 鼠标侧键热键运行时状态
  HHOOK mouse_hotkey_hook = nullptr;
  HWND mouse_hotkey_target_hwnd = nullptr;
  std::unordered_map<std::uint32_t, int> mouse_combo_to_hotkey_id;  // combo -> hotkey_id
};

}  // namespace core::commands
