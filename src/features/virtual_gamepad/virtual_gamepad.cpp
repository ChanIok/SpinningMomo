module;

#include <windows.h>

#include <ViGEm/Client.h>

module Features.VirtualGamepad;

import std;
import Core.State;
import Features.VirtualGamepad.State;
import Features.Settings.State;
import Features.WindowControl;
import Utils.Logger;
import Utils.String;

namespace Features::VirtualGamepad {

// 全局状态指针，用于键盘钩子回调
Core::State::AppState* g_app_state = nullptr;

// ========== ViGEm 操作 ==========

auto connect_vigem(State::VirtualGamepadState& state) -> std::expected<void, std::string> {
  auto client = vigem_alloc();
  if (!client) {
    return std::unexpected("Failed to allocate ViGEm client");
  }

  auto ret = vigem_connect(client);
  if (!VIGEM_SUCCESS(ret)) {
    vigem_free(client);
    return std::unexpected(std::format("Failed to connect to ViGEm bus, error: 0x{:08X}",
                                       static_cast<unsigned int>(ret)));
  }

  state.vigem.client = client;
  state.vigem_available = true;
  Logger().info("ViGEm client connected successfully");
  return {};
}

auto disconnect_vigem(State::VirtualGamepadState& state) -> void {
  if (state.vigem.client) {
    vigem_disconnect(static_cast<PVIGEM_CLIENT>(state.vigem.client));
    vigem_free(static_cast<PVIGEM_CLIENT>(state.vigem.client));
    state.vigem.client = nullptr;
  }
  state.vigem_available = false;
}

auto create_gamepad(State::VirtualGamepadState& state) -> std::expected<void, std::string> {
  if (!state.vigem.client) {
    return std::unexpected("ViGEm client not connected");
  }

  auto target = vigem_target_x360_alloc();
  if (!target) {
    return std::unexpected("Failed to allocate Xbox 360 controller target");
  }

  auto ret = vigem_target_add(static_cast<PVIGEM_CLIENT>(state.vigem.client), target);
  if (!VIGEM_SUCCESS(ret)) {
    vigem_target_free(target);
    return std::unexpected(std::format("Failed to add virtual gamepad, error: 0x{:08X}",
                                       static_cast<unsigned int>(ret)));
  }

  state.vigem.target = target;
  Logger().info("Virtual Xbox 360 controller created");
  return {};
}

auto remove_gamepad(State::VirtualGamepadState& state) -> void {
  if (state.vigem.target && state.vigem.client) {
    vigem_target_remove(static_cast<PVIGEM_CLIENT>(state.vigem.client),
                        static_cast<PVIGEM_TARGET>(state.vigem.target));
    vigem_target_free(static_cast<PVIGEM_TARGET>(state.vigem.target));
    state.vigem.target = nullptr;
    Logger().info("Virtual gamepad removed");
  }
}

// 计算摇杆目标值（从 4 个方向键计算归一化后的 X/Y 目标）
auto calculate_joystick_target(const State::JoystickState& js) -> std::pair<float, float> {
  float x = (js.right ? 1.0f : 0.0f) - (js.left ? 1.0f : 0.0f);
  float y = (js.up ? 1.0f : 0.0f) - (js.down ? 1.0f : 0.0f);

  // 归一化对角线移动（避免超过 1）
  float len = std::sqrt(x * x + y * y);
  if (len > 1.0f) {
    x /= len;
    y /= len;
  }

  return {x, y};
}

auto update_gamepad(State::VirtualGamepadState& state) -> void {
  if (!state.vigem.client || !state.vigem.target) {
    return;
  }

  XUSB_REPORT report;
  XUSB_REPORT_INIT(&report);

  // === 扳机（平滑值 + 幅度缩放） ===
  report.bLeftTrigger = static_cast<BYTE>(
      std::clamp(state.input.left_trigger.value * state.speed_config.trigger_scale, 0.0f, 255.0f));
  report.bRightTrigger = static_cast<BYTE>(
      std::clamp(state.input.right_trigger.value * state.speed_config.trigger_scale, 0.0f, 255.0f));

  // === 摇杆（平滑值 + 幅度缩放） ===
  float js_scale = state.speed_config.joystick_scale;
  report.sThumbLX = static_cast<SHORT>(state.input.left_stick.x_value * js_scale * 32767);
  report.sThumbLY = static_cast<SHORT>(state.input.left_stick.y_value * js_scale * 32767);
  report.sThumbRX = static_cast<SHORT>(state.input.right_stick.x_value * js_scale * 32767);
  report.sThumbRY = static_cast<SHORT>(state.input.right_stick.y_value * js_scale * 32767);

  // === 按键位掩码 ===
  USHORT buttons = 0;

  // 面键
  if (state.input.button_a) buttons |= XUSB_GAMEPAD_A;
  if (state.input.button_b) buttons |= XUSB_GAMEPAD_B;
  if (state.input.button_x) buttons |= XUSB_GAMEPAD_X;
  if (state.input.button_y) buttons |= XUSB_GAMEPAD_Y;

  // 肩键
  if (state.input.left_bumper) buttons |= XUSB_GAMEPAD_LEFT_SHOULDER;
  if (state.input.right_bumper) buttons |= XUSB_GAMEPAD_RIGHT_SHOULDER;

  // D-pad
  if (state.input.dpad_up) buttons |= XUSB_GAMEPAD_DPAD_UP;
  if (state.input.dpad_down) buttons |= XUSB_GAMEPAD_DPAD_DOWN;
  if (state.input.dpad_left) buttons |= XUSB_GAMEPAD_DPAD_LEFT;
  if (state.input.dpad_right) buttons |= XUSB_GAMEPAD_DPAD_RIGHT;

  // 功能键
  if (state.input.start) buttons |= XUSB_GAMEPAD_START;
  if (state.input.back) buttons |= XUSB_GAMEPAD_BACK;

  // 摇杆按下
  if (state.input.left_thumb) buttons |= XUSB_GAMEPAD_LEFT_THUMB;
  if (state.input.right_thumb) buttons |= XUSB_GAMEPAD_RIGHT_THUMB;

  report.wButtons = buttons;

  vigem_target_x360_update(static_cast<PVIGEM_CLIENT>(state.vigem.client),
                           static_cast<PVIGEM_TARGET>(state.vigem.target), report);
}

// ========== 键盘钩子 ==========

// 更新单个 bool 状态
auto update_key_state(bool& state, bool key_down, bool key_up) -> void {
  if (key_down) {
    state = true;
  } else if (key_up) {
    state = false;
  }
}

LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wParam, LPARAM lParam) {
  if (code >= 0 && g_app_state && g_app_state->virtual_gamepad) {
    auto& state = *g_app_state->virtual_gamepad;
    auto& mapping = state.key_mapping;
    auto& input = state.input;
    auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    bool key_down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    bool key_up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
    DWORD vk = kb->vkCode;
    bool matched = false;

    // === 左摇杆: W / A / S / D ===
    if (vk == mapping.left_stick.up) {
      update_key_state(input.left_stick.up, key_down, key_up);
      matched = true;
    } else if (vk == mapping.left_stick.down) {
      update_key_state(input.left_stick.down, key_down, key_up);
      matched = true;
    } else if (vk == mapping.left_stick.left) {
      update_key_state(input.left_stick.left, key_down, key_up);
      matched = true;
    } else if (vk == mapping.left_stick.right) {
      update_key_state(input.left_stick.right, key_down, key_up);
      matched = true;
    }
    // === 右摇杆: I / J / K / L ===
    else if (vk == mapping.right_stick.up) {
      update_key_state(input.right_stick.up, key_down, key_up);
      matched = true;
    } else if (vk == mapping.right_stick.down) {
      update_key_state(input.right_stick.down, key_down, key_up);
      matched = true;
    } else if (vk == mapping.right_stick.left) {
      update_key_state(input.right_stick.left, key_down, key_up);
      matched = true;
    } else if (vk == mapping.right_stick.right) {
      update_key_state(input.right_stick.right, key_down, key_up);
      matched = true;
    }
    // === 扳机: Q / E ===
    else if (vk == mapping.left_trigger) {
      update_key_state(input.left_trigger.pressed, key_down, key_up);
      matched = true;
    } else if (vk == mapping.right_trigger) {
      update_key_state(input.right_trigger.pressed, key_down, key_up);
      matched = true;
    }
    // === 肩键: Z / X ===
    else if (vk == mapping.left_bumper) {
      update_key_state(input.left_bumper, key_down, key_up);
      matched = true;
    } else if (vk == mapping.right_bumper) {
      update_key_state(input.right_bumper, key_down, key_up);
      matched = true;
    }
    // === 面键: 1 / 2 / 3 / 4 ===
    else if (vk == mapping.button_a) {
      update_key_state(input.button_a, key_down, key_up);
      matched = true;
    } else if (vk == mapping.button_b) {
      update_key_state(input.button_b, key_down, key_up);
      matched = true;
    } else if (vk == mapping.button_x) {
      update_key_state(input.button_x, key_down, key_up);
      matched = true;
    } else if (vk == mapping.button_y) {
      update_key_state(input.button_y, key_down, key_up);
      matched = true;
    }
    // === D-pad: 方向键 ===
    else if (vk == mapping.dpad_up) {
      update_key_state(input.dpad_up, key_down, key_up);
      matched = true;
    } else if (vk == mapping.dpad_down) {
      update_key_state(input.dpad_down, key_down, key_up);
      matched = true;
    } else if (vk == mapping.dpad_left) {
      update_key_state(input.dpad_left, key_down, key_up);
      matched = true;
    } else if (vk == mapping.dpad_right) {
      update_key_state(input.dpad_right, key_down, key_up);
      matched = true;
    }
    // === 功能键: Enter / Backspace ===
    else if (vk == mapping.start) {
      update_key_state(input.start, key_down, key_up);
      matched = true;
    } else if (vk == mapping.back) {
      update_key_state(input.back, key_down, key_up);
      matched = true;
    }
    // === 摇杆按下: C / V ===
    else if (vk == mapping.left_thumb) {
      update_key_state(input.left_thumb, key_down, key_up);
      matched = true;
    } else if (vk == mapping.right_thumb) {
      update_key_state(input.right_thumb, key_down, key_up);
      matched = true;
    }

    // 如果按键被映射且游戏窗口在前台，吃掉按键，不让游戏收到键盘输入
    if (matched && state.game_hwnd && GetForegroundWindow() == state.game_hwnd) {
      return 1;
    }
  }
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

auto install_keyboard_hook(State::VirtualGamepadState& state) -> std::expected<void, std::string> {
  if (state.keyboard_hook) {
    return {};  // 已安装
  }

  state.keyboard_hook =
      SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_proc, GetModuleHandle(nullptr), 0);
  if (!state.keyboard_hook) {
    return std::unexpected(
        std::format("Failed to install keyboard hook, error: {}", GetLastError()));
  }

  Logger().info("Keyboard hook installed for virtual gamepad");
  return {};
}

auto uninstall_keyboard_hook(State::VirtualGamepadState& state) -> void {
  if (state.keyboard_hook) {
    UnhookWindowsHookEx(state.keyboard_hook);
    state.keyboard_hook = nullptr;
    Logger().info("Keyboard hook uninstalled");
  }
}

// ========== 更新循环 ==========

auto update_trigger_value(State::TriggerState& trigger, const State::SpeedConfig& config, float dt)
    -> void {
  float target = trigger.pressed ? static_cast<float>(config.max_value) : 0.0f;

  if (trigger.value < target) {
    trigger.value = std::min(trigger.value + config.accel_rate * dt, target);
  } else if (trigger.value > target) {
    trigger.value = std::max(trigger.value - config.decel_rate * dt, target);
  }
}

// 更新摇杆值（平滑插值，范围 -1.0 到 1.0）
auto update_joystick_value(State::JoystickState& js, const State::SpeedConfig& config, float dt)
    -> void {
  // 计算目标值（已归一化）
  auto [target_x, target_y] = calculate_joystick_target(js);

  // 平滑插值率（使用与扳机相同的速率，但映射到 -1~1 范围）
  // accel_rate 是 500/秒（基于 0-255），映射到 0-1 约为 2/秒
  float rate = config.accel_rate / 255.0f;

  // X 轴插值
  if (js.x_value < target_x) {
    js.x_value = std::min(js.x_value + rate * dt, target_x);
  } else if (js.x_value > target_x) {
    js.x_value = std::max(js.x_value - rate * dt, target_x);
  }

  // Y 轴插值
  if (js.y_value < target_y) {
    js.y_value = std::min(js.y_value + rate * dt, target_y);
  } else if (js.y_value > target_y) {
    js.y_value = std::max(js.y_value - rate * dt, target_y);
  }
}

auto start_update_loop(Core::State::AppState& state) -> void {
  auto& vg_state = *state.virtual_gamepad;

  if (vg_state.update_running) {
    return;
  }

  vg_state.update_running = true;

  vg_state.update_thread = std::jthread([&vg_state](std::stop_token stop_token) {
    Logger().info("Virtual gamepad update loop started");

    constexpr auto update_interval = std::chrono::microseconds(8333);  // ~120Hz
    auto last_time = std::chrono::steady_clock::now();

    while (!stop_token.stop_requested()) {
      auto now = std::chrono::steady_clock::now();
      float dt = std::chrono::duration<float>(now - last_time).count();
      last_time = now;

      // 更新扳机值（平滑插值）
      update_trigger_value(vg_state.input.left_trigger, vg_state.speed_config, dt);
      update_trigger_value(vg_state.input.right_trigger, vg_state.speed_config, dt);

      // 更新摇杆值（平滑插值）
      update_joystick_value(vg_state.input.left_stick, vg_state.speed_config, dt);
      update_joystick_value(vg_state.input.right_stick, vg_state.speed_config, dt);

      // 推送到 ViGEm
      update_gamepad(vg_state);

      std::this_thread::sleep_for(update_interval);
    }

    Logger().info("Virtual gamepad update loop stopped");
  });
}

auto stop_update_loop(State::VirtualGamepadState& state) -> void {
  if (!state.update_running) {
    return;
  }

  state.update_running = false;
  if (state.update_thread.joinable()) {
    state.update_thread.request_stop();
    state.update_thread.join();
  }
}

// ========== 公开接口 ==========

auto initialize(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& vg_state = *state.virtual_gamepad;

  g_app_state = &state;

  // 尝试连接 ViGEm，检测驱动是否可用
  auto result = connect_vigem(vg_state);
  if (!result) {
    // 连接失败不是致命错误，只是标记为不可用
    Logger().warn("ViGEm not available: {}", result.error());
    vg_state.vigem_available = false;
    return {};  // 仍然返回成功，允许应用继续运行
  }

  Logger().info("Virtual gamepad feature initialized (ViGEm available)");
  return {};
}

auto shutdown(Core::State::AppState& state) -> void {
  if (!state.virtual_gamepad) {
    return;
  }

  auto& vg_state = *state.virtual_gamepad;

  // 禁用功能（会停止更新循环、卸载钩子、移除手柄）
  disable(state);

  // 断开 ViGEm 连接
  disconnect_vigem(vg_state);

  g_app_state = nullptr;
  Logger().info("Virtual gamepad feature shutdown");
}

auto enable(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& vg_state = *state.virtual_gamepad;

  if (vg_state.enabled) {
    return {};  // 已启用
  }

  if (!vg_state.vigem_available) {
    return std::unexpected("ViGEm driver not available. Please install ViGEmBus v1.22.0 or later.");
  }

  // 查找并缓存游戏窗口句柄
  std::wstring window_title = Utils::String::FromUtf8(state.settings->raw.window.target_title);
  auto target_window = Features::WindowControl::find_target_window(window_title);
  if (target_window) {
    vg_state.game_hwnd = target_window.value();
    Logger().info("Game window found for virtual gamepad key suppression");
  } else {
    vg_state.game_hwnd = nullptr;
    Logger().warn("Game window not found, keyboard keys will not be suppressed");
  }

  // 创建虚拟手柄
  if (auto result = create_gamepad(vg_state); !result) {
    return result;
  }

  // 安装键盘钩子
  if (auto result = install_keyboard_hook(vg_state); !result) {
    remove_gamepad(vg_state);
    return result;
  }

  // 启动更新循环
  start_update_loop(state);

  vg_state.enabled = true;
  Logger().info("Virtual gamepad enabled");
  return {};
}

auto disable(Core::State::AppState& state) -> void {
  if (!state.virtual_gamepad) {
    return;
  }

  auto& vg_state = *state.virtual_gamepad;

  if (!vg_state.enabled) {
    return;
  }

  // 停止更新循环
  stop_update_loop(vg_state);

  // 卸载键盘钩子
  uninstall_keyboard_hook(vg_state);

  // 移除虚拟手柄
  remove_gamepad(vg_state);

  // 重置输入状态
  vg_state.input = State::InputState{};

  // 清除游戏窗口句柄
  vg_state.game_hwnd = nullptr;

  vg_state.enabled = false;
  Logger().info("Virtual gamepad disabled");
}

auto toggle(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.virtual_gamepad) {
    return std::unexpected("Virtual gamepad state not initialized");
  }

  if (state.virtual_gamepad->enabled) {
    disable(state);
    return {};
  } else {
    return enable(state);
  }
}

auto is_available(const Core::State::AppState& state) -> bool {
  return state.virtual_gamepad && state.virtual_gamepad->vigem_available;
}

auto is_enabled(const Core::State::AppState& state) -> bool {
  return state.virtual_gamepad && state.virtual_gamepad->enabled;
}

}  // namespace Features::VirtualGamepad
