module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.PluginEndpoints;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Plugins.InfinityNikki;
import Plugins.InfinityNikki.Types;
import Utils.Logger;

namespace Core::RPC::Endpoints::PluginEndpoints {

auto handle_infinity_nikki_get_game_directory(
    [[maybe_unused]] Core::State::AppState& app_state,
    const Plugins::InfinityNikki::InfinityNikkiGameDirRequest& params)
    -> asio::awaitable<Core::RPC::RpcResult<Plugins::InfinityNikki::InfinityNikkiGameDirResult>> {
  auto result = Plugins::InfinityNikki::get_game_directory();
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Failed to get Infinity Nikki game directory: " + result.error(),
    });
  }

  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<Plugins::InfinityNikki::InfinityNikkiGameDirRequest,
                             Plugins::InfinityNikki::InfinityNikkiGameDirResult>(
      app_state, app_state.rpc->registry, "plugins.infinityNikki.getGameDirectory",
      handle_infinity_nikki_get_game_directory,
      "Get Infinity Nikki game installation directory from launcher config");

  Logger().info("Plugins RPC endpoints registered");
}

}  // namespace Core::RPC::Endpoints::PluginEndpoints