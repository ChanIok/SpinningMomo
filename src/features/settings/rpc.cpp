module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Features.Settings.Rpc;

import std;
import Core.State;
import Core.RpcHandlers;
import Core.RpcHandlers.Types;
import Core.RpcHandlers.State;
import Features.Settings;
import Features.Settings.Types;
import Utils.Logger;

namespace Features::Settings::Rpc {

auto handle_get_settings([[maybe_unused]] Core::State::AppState& app_state,
                         const Types::GetSettingsParams& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<Types::GetSettingsResult>> {
  auto result = Features::Settings::get_settings(params);

  if (result) {
    co_return result.value();
  } else {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Service error: " + result.error()});
  }
}

auto handle_update_settings(Core::State::AppState& app_state,
                            const Types::UpdateSettingsParams& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<Types::UpdateSettingsResult>> {
  auto result = Features::Settings::update_settings(app_state, params);

  if (result) {
    co_return result.value();
  } else {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Service error: " + result.error()});
  }
}

auto register_handlers(Core::State::AppState& app_state) -> void {
  Core::RpcHandlers::register_method<Types::GetSettingsParams, Types::GetSettingsResult>(
      app_state, app_state.rpc_handlers->registry, "settings.get", handle_get_settings,
      "Get current settings configuration");

  Core::RpcHandlers::register_method<Types::UpdateSettingsParams, Types::UpdateSettingsResult>(
      app_state, app_state.rpc_handlers->registry, "settings.update", handle_update_settings,
      "Update settings configuration");

  Logger().info("Registered settings RPC methods");
}

}  // namespace Features::Settings::Rpc