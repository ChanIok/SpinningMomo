module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Features.Screenshot.Folder.Rpc;

import std;
import Core.State;
import Core.RpcHandlers;
import Core.RpcHandlers.Types;
import Core.RpcHandlers.State;
import Features.Screenshot.Folder;
import Features.Screenshot.Folder.Types;
import Utils.Logger;

namespace Features::Screenshot::Folder::Rpc {

auto handle_select([[maybe_unused]] Core::State::AppState& app_state,
                   const Features::Screenshot::Folder::Types::SelectParams& /*params*/)
    -> asio::awaitable<
        Core::RpcHandlers::RpcResult<Features::Screenshot::Folder::Types::SelectResult>> {
  auto result = Features::Screenshot::Folder::select_folder(app_state);
  if (result) {
    co_return Features::Screenshot::Folder::Types::SelectResult{.path = result.value().string()};
  }
  co_return std::unexpected(Core::RpcHandlers::RpcError{
      .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
      .message = "Service error: " + result.error(),
  });
}

auto register_handlers(Core::State::AppState& app_state) -> void {
  Core::RpcHandlers::register_method<Features::Screenshot::Folder::Types::SelectParams,
                                     Features::Screenshot::Folder::Types::SelectResult>(
      app_state, app_state.rpc_handlers->registry, "screenshot.folder.select", handle_select,
      "Open a folder picker and return selected path");

  Logger().info("Registered screenshot folder RPC methods: select");
}

}  // namespace Features::Screenshot::Folder::Rpc
