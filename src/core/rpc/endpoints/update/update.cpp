module;

module Core.RPC.Endpoints.Update;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Update;
import Features.Update.Types;
import <asio.hpp>;
import <rfl.hpp>;

namespace Core::RPC::Endpoints::Update {

auto handle_check_for_update(Core::State::AppState& app_state,
                             [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Update::Types::CheckUpdateResult>> {
  auto result = co_await Features::Update::check_for_update(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_start_download(Core::State::AppState& app_state,
                           [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Update::Types::StartDownloadUpdateResult>> {
  auto result = co_await Features::Update::start_download_update_task(app_state);

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

  Core::RPC::register_method<rfl::Generic, Features::Update::Types::StartDownloadUpdateResult>(
      app_state, app_state.rpc->registry, "update.start_download", handle_start_download,
      "Start downloading update package in background");

  Core::RPC::register_method<Features::Update::Types::InstallUpdateParams,
                             Features::Update::Types::InstallUpdateResult>(
      app_state, app_state.rpc->registry, "update.install_update", handle_install_update,
      "Install downloaded update");
}

}  // namespace Core::RPC::Endpoints::Update
