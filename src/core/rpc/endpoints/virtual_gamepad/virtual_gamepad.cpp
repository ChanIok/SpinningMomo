module;

module Core.RPC.Endpoints.VirtualGamepad;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.RPC.Endpoints.VirtualGamepad.Types;
import Features.VirtualGamepad;
import Features.VirtualGamepad.State;
import <asio.hpp>;

namespace Core::RPC::Endpoints::VirtualGamepad {

using namespace Types;

auto handle_get_status(Core::State::AppState& app_state,
                       [[maybe_unused]] const GetStatusParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<GetStatusResult>> {
  auto& state = *app_state.virtual_gamepad;

  co_return GetStatusResult{
      .vigem_available = state.vigem_available,
      .enabled = state.enabled,
      .left_trigger_key = state.key_mapping.left_trigger,
      .right_trigger_key = state.key_mapping.right_trigger,
      .accel_rate = state.speed_config.accel_rate,
      .decel_rate = state.speed_config.decel_rate,
      .trigger_scale = state.speed_config.trigger_scale,
      .joystick_scale = state.speed_config.joystick_scale,
  };
}

auto handle_toggle(Core::State::AppState& app_state, [[maybe_unused]] const ToggleParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<ToggleResult>> {
  auto result = Features::VirtualGamepad::toggle(app_state);

  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError), .message = result.error()});
  }

  bool enabled = Features::VirtualGamepad::is_enabled(app_state);
  co_return ToggleResult{
      .enabled = enabled,
      .message = enabled ? "Virtual gamepad enabled" : "Virtual gamepad disabled",
  };
}

auto handle_update_config(Core::State::AppState& app_state, const UpdateConfigParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<UpdateConfigResult>> {
  auto& state = *app_state.virtual_gamepad;

  // 更新配置（只更新提供的字段）
  if (params.left_trigger_key) {
    state.key_mapping.left_trigger = *params.left_trigger_key;
  }
  if (params.right_trigger_key) {
    state.key_mapping.right_trigger = *params.right_trigger_key;
  }
  if (params.accel_rate) {
    state.speed_config.accel_rate = *params.accel_rate;
  }
  if (params.decel_rate) {
    state.speed_config.decel_rate = *params.decel_rate;
  }
  if (params.trigger_scale) {
    state.speed_config.trigger_scale = std::clamp(*params.trigger_scale, 0.0f, 1.0f);
  }
  if (params.joystick_scale) {
    state.speed_config.joystick_scale = std::clamp(*params.joystick_scale, 0.0f, 1.0f);
  }

  co_return UpdateConfigResult{
      .success = true,
      .message = "Configuration updated",
  };
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<GetStatusParams, GetStatusResult>(
      app_state, app_state.rpc->registry, "virtual_gamepad.get_status", handle_get_status,
      "Get virtual gamepad status and configuration");

  Core::RPC::register_method<ToggleParams, ToggleResult>(app_state, app_state.rpc->registry,
                                                         "virtual_gamepad.toggle", handle_toggle,
                                                         "Toggle virtual gamepad enabled/disabled");

  Core::RPC::register_method<UpdateConfigParams, UpdateConfigResult>(
      app_state, app_state.rpc->registry, "virtual_gamepad.update_config", handle_update_config,
      "Update virtual gamepad configuration");
}

}  // namespace Core::RPC::Endpoints::VirtualGamepad
