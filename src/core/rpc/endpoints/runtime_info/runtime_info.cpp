module;

#include <asio.hpp>

module Core.RPC.Endpoints.RuntimeInfo;

import std;
import Core.State;
import Core.State.RuntimeInfo;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;

namespace Core::RPC::Endpoints::RuntimeInfo {

struct GetRuntimeInfoParams {};

using GetRuntimeInfoResult = Core::State::RuntimeInfo::RuntimeInfoState;

auto handle_get_runtime_info(Core::State::AppState& app_state,
                             [[maybe_unused]] const GetRuntimeInfoParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<GetRuntimeInfoResult>> {
  if (!app_state.runtime_info) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Runtime info state not initialized"});
  }

  co_return *app_state.runtime_info;
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<GetRuntimeInfoParams, GetRuntimeInfoResult>(
      app_state, app_state.rpc->registry, "runtime_info.get", handle_get_runtime_info,
      "Get application runtime info and capability flags");
}

}  // namespace Core::RPC::Endpoints::RuntimeInfo
