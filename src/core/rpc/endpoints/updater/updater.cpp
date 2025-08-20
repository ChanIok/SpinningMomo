module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.Updater;

import std;
import Core.State;
import Core.RPC.Engine;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Updater;
import Features.Updater.Types;

namespace Core::RPC::Endpoints::Updater {

auto handle_check_for_update(Core::State::AppState& app_state,
                             [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Updater::Types::CheckUpdateResult>> {
  auto result = Features::Updater::check_for_update(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_download_update(Core::State::AppState& app_state,
                            [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Updater::Types::DownloadUpdateResult>> {
  auto result = Features::Updater::download_update(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_install_update(Core::State::AppState& app_state,
                           const Features::Updater::Types::InstallUpdateParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Updater::Types::InstallUpdateResult>> {
  auto result = Features::Updater::install_update(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<rfl::Generic, Features::Updater::Types::CheckUpdateResult>(
      app_state, app_state.rpc->registry, "updater.check_for_update", handle_check_for_update,
      "Check for available updates");

  Core::RPC::register_method<rfl::Generic, Features::Updater::Types::DownloadUpdateResult>(
      app_state, app_state.rpc->registry, "updater.download_update", handle_download_update,
      "Download update package");

  Core::RPC::register_method<Features::Updater::Types::InstallUpdateParams,
                             Features::Updater::Types::InstallUpdateResult>(
      app_state, app_state.rpc->registry, "updater.install_update", handle_install_update,
      "Install downloaded update");
}

}  // namespace Core::RPC::Endpoints::Updater