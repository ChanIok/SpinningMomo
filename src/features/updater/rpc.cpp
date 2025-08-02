module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Features.Updater.Rpc;

import std;
import Core.State;
import Core.RpcHandlers;
import Core.RpcHandlers.Types;
import Core.RpcHandlers.State;
import Features.Updater;
import Features.Updater.Types;
import Utils.Logger;

namespace Features::Updater::Rpc {

auto handle_check_for_update(Core::State::AppState& app_state,
                             [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<Types::CheckUpdateResult>> {
  auto result = Features::Updater::check_for_update(app_state);

  if (result) {
    co_return result.value();
  } else {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Service error: " + result.error()});
  }
}

auto handle_download_update(Core::State::AppState& app_state,
                            [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<Types::DownloadUpdateResult>> {
  auto result = Features::Updater::download_update(app_state);

  if (result) {
    co_return result.value();
  } else {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Service error: " + result.error()});
  }
}

auto handle_install_update(Core::State::AppState& app_state,
                           const Types::InstallUpdateParams& params)
    -> asio::awaitable<Core::RpcHandlers::RpcResult<Types::InstallUpdateResult>> {
  auto result = Features::Updater::install_update(app_state, params);

  if (result) {
    co_return result.value();
  } else {
    co_return std::unexpected(Core::RpcHandlers::RpcError{
        .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
        .message = "Service error: " + result.error()});
  }
}

auto register_handlers(Core::State::AppState& app_state) -> void {
  Core::RpcHandlers::register_method<rfl::Generic, Types::CheckUpdateResult>(
      app_state, app_state.rpc_handlers->registry, "updater.check_for_update",
      handle_check_for_update, "Check for available updates");

  Core::RpcHandlers::register_method<rfl::Generic, Types::DownloadUpdateResult>(
      app_state, app_state.rpc_handlers->registry, "updater.download_update",
      handle_download_update, "Download update package");

  Core::RpcHandlers::register_method<Types::InstallUpdateParams, Types::InstallUpdateResult>(
      app_state, app_state.rpc_handlers->registry, "updater.install_update", handle_install_update,
      "Install downloaded update");

  Logger().info("Registered updater RPC methods");
}

}  // namespace Features::Updater::Rpc