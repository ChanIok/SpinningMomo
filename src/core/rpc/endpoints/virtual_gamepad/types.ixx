module;

export module Core.RPC.Endpoints.VirtualGamepad.Types;

import std;
import <cstdint>;

export namespace Core::RPC::Endpoints::VirtualGamepad::Types {

// ===== 获取状态 =====

struct GetStatusParams {};

struct GetStatusResult {
  bool vigem_available;  // ViGEm 驱动是否可用
  bool enabled;          // 功能是否已启用
  std::uint32_t left_trigger_key;
  std::uint32_t right_trigger_key;
  float accel_rate;
  float decel_rate;
  float trigger_scale;   // 扳机幅度
  float joystick_scale;  // 摇杆幅度
};

// ===== 切换启用/禁用 =====

struct ToggleParams {};

struct ToggleResult {
  bool enabled;  // 切换后的状态
  std::string message;
};

// ===== 更新配置 =====

struct UpdateConfigParams {
  std::optional<std::uint32_t> left_trigger_key;
  std::optional<std::uint32_t> right_trigger_key;
  std::optional<float> accel_rate;
  std::optional<float> decel_rate;
  std::optional<float> trigger_scale;
  std::optional<float> joystick_scale;
};

struct UpdateConfigResult {
  bool success;
  std::string message;
};

}  // namespace Core::RPC::Endpoints::VirtualGamepad::Types
