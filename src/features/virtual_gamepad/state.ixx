module;

export module Features.VirtualGamepad.State;

import std;
import <windows.h>;

export namespace Features::VirtualGamepad::State {

// ========== 按键映射配置 ==========

// 摇杆方向映射（4 个方向键）
struct JoystickMapping {
  UINT up;
  UINT down;
  UINT left;
  UINT right;
};

// 完整的按键映射配置
struct KeyMapping {
  // 左摇杆: W / A / S / D
  JoystickMapping left_stick{'W', 'S', 'A', 'D'};

  // 右摇杆: I / J / K / L
  JoystickMapping right_stick{'I', 'K', 'J', 'L'};

  // 扳机: Q / E
  UINT left_trigger{'Q'};
  UINT right_trigger{'E'};

  // 肩键: Z / X
  UINT left_bumper{'Z'};
  UINT right_bumper{'X'};

  // 面键: 1 / 2 / 3 / 4 -> A / B / X / Y
  UINT button_a{'1'};
  UINT button_b{'2'};
  UINT button_x{'3'};
  UINT button_y{'4'};

  // D-pad: 方向键
  UINT dpad_up{VK_UP};
  UINT dpad_down{VK_DOWN};
  UINT dpad_left{VK_LEFT};
  UINT dpad_right{VK_RIGHT};

  // 功能键: Enter / Backspace -> Start / Back
  UINT start{VK_RETURN};
  UINT back{VK_BACK};

  // 摇杆按下: C / V -> L3 / R3
  UINT left_thumb{'C'};
  UINT right_thumb{'V'};
};

// 速度曲线配置（用于扳机和摇杆）
struct SpeedConfig {
  float accel_rate{500.0f};  // 按下时每秒增加量（约 0.5s 到满）
  float decel_rate{500.0f};  // 松开时每秒减少量
  BYTE max_value{255};       // 扳机最大值

  // 幅度缩放（0.0 - 1.0）
  float trigger_scale{0.5f};   // 扳机幅度：20% 默认
  float joystick_scale{0.5f};  // 摇杆幅度：50% 默认
};

// ViGEm 相关句柄
struct ViGEmHandles {
  void* client{nullptr};  // PVIGEM_CLIENT
  void* target{nullptr};  // PVIGEM_TARGET
};

// ========== 输入状态 ==========

// 摇杆状态（4 个方向的按下状态 + 平滑值）
struct JoystickState {
  // 按键状态
  bool up{false};
  bool down{false};
  bool left{false};
  bool right{false};

  // 平滑后的轴值（-1.0 到 1.0）
  float x_value{0.0f};
  float y_value{0.0f};
};

// 扳机状态（带平滑值）
struct TriggerState {
  bool pressed{false};
  float value{0.0f};  // 0-255 的浮点值，用于平滑插值
};

// 完整的输入状态
struct InputState {
  // 摇杆
  JoystickState left_stick;
  JoystickState right_stick;

  // 扳机（带平滑）
  TriggerState left_trigger;
  TriggerState right_trigger;

  // 肩键
  bool left_bumper{false};
  bool right_bumper{false};

  // 面键
  bool button_a{false};
  bool button_b{false};
  bool button_x{false};
  bool button_y{false};

  // D-pad
  bool dpad_up{false};
  bool dpad_down{false};
  bool dpad_left{false};
  bool dpad_right{false};

  // 功能键
  bool start{false};
  bool back{false};

  // 摇杆按下
  bool left_thumb{false};
  bool right_thumb{false};
};

// ========== 主状态 ==========

struct VirtualGamepadState {
  // 功能开关
  bool enabled{false};
  bool vigem_available{false};  // ViGEm 驱动是否可用

  // ViGEm 句柄
  ViGEmHandles vigem;

  // 配置
  KeyMapping key_mapping;
  SpeedConfig speed_config;

  // 输入状态
  InputState input;

  // 键盘钩子
  HHOOK keyboard_hook{nullptr};

  // 游戏窗口句柄（用于判断前台窗口，吃掉映射按键）
  HWND game_hwnd{nullptr};

  // 更新循环相关
  bool update_running{false};
  std::jthread update_thread;
};

}  // namespace Features::VirtualGamepad::State
