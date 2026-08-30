#include "core/rpc/endpoints/adb_mode/adb_mode.hpp"

#include "vendor/std.hpp"

#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "features/adb_mode/types.hpp"
#include "features/adb_mode/usecase.hpp"

namespace core::rpc::endpoints::adb_mode {

namespace {

auto handle_get_status(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> core::rpc::RpcAwaitable<features::adb_mode::AdbModeStatus> {
  co_return features::adb_mode::get_status(app_state);
}

}  // namespace

auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<EmptyParams, features::adb_mode::AdbModeStatus>(
      app_state, app_state.rpc->registry, "adbMode.getStatus", handle_get_status,
      "Get the ADB connection and display status");
}

}  // namespace core::rpc::endpoints::adb_mode
