#include "core/rpc/endpoints/runtime_info/runtime_info.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "core/state/runtime_info.hpp"

namespace core::rpc::endpoints::runtime_info {

struct GetRuntimeInfoParams {};

using GetRuntimeInfoResult = core::runtime_info::RuntimeInfoState;

auto handle_get_runtime_info(core::AppState& app_state,
                             [[maybe_unused]] const GetRuntimeInfoParams& params)
    -> asio::awaitable<core::rpc::RpcResult<GetRuntimeInfoResult>> {
  if (!app_state.runtime_info) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Runtime info state not initialized"});
  }

  co_return *app_state.runtime_info;
}

auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<GetRuntimeInfoParams, GetRuntimeInfoResult>(
      app_state, app_state.rpc->registry, "runtime_info.get", handle_get_runtime_info,
      "Get application runtime info and capability flags");
}

}  // namespace core::rpc::endpoints::runtime_info
