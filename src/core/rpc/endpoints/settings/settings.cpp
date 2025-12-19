module;

module Core.RPC.Endpoints.Settings;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Settings;
import Features.Settings.Types;
import <asio.hpp>;

namespace Core::RPC::Endpoints::Settings {

auto handle_get_settings([[maybe_unused]] Core::State::AppState& app_state,
                         const Features::Settings::Types::GetSettingsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Settings::Types::GetSettingsResult>> {
  auto result = Features::Settings::get_settings(params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_update_settings(Core::State::AppState& app_state,
                            const Features::Settings::Types::UpdateSettingsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Settings::Types::UpdateSettingsResult>> {
  auto result = Features::Settings::update_settings(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<Features::Settings::Types::GetSettingsParams,
                             Features::Settings::Types::GetSettingsResult>(
      app_state, app_state.rpc->registry, "settings.get", handle_get_settings,
      "Get current settings configuration");

  Core::RPC::register_method<Features::Settings::Types::UpdateSettingsParams,
                             Features::Settings::Types::UpdateSettingsResult>(
      app_state, app_state.rpc->registry, "settings.update", handle_update_settings,
      "Update settings configuration");
}

}  // namespace Core::RPC::Endpoints::Settings
