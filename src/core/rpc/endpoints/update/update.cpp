module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.Update;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Update;
import Features.Update.Types;

namespace Core::RPC::Endpoints::Update {

auto handle_check_for_update(Core::State::AppState& app_state,
                             [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Update::Types::CheckUpdateResult>> {
  auto result = Features::Update::check_for_update(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_download_update(Core::State::AppState& app_state,
                            [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Update::Types::DownloadUpdateResult>> {
  auto result = Features::Update::download_update(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_install_update(Core::State::AppState& app_state,
                           const Features::Update::Types::InstallUpdateParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Update::Types::InstallUpdateResult>> {
  auto result = Features::Update::install_update(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<rfl::Generic, Features::Update::Types::CheckUpdateResult>(
      app_state, app_state.rpc->registry, "update.check_for_update", handle_check_for_update,
      "Check for available updates");

  Core::RPC::register_method<rfl::Generic, Features::Update::Types::DownloadUpdateResult>(
      app_state, app_state.rpc->registry, "update.download_update", handle_download_update,
      "Download update package");

  Core::RPC::register_method<Features::Update::Types::InstallUpdateParams,
                             Features::Update::Types::InstallUpdateResult>(
      app_state, app_state.rpc->registry, "update.install_update", handle_install_update,
      "Install downloaded update");
}

}  // namespace Core::RPC::Endpoints::Update
