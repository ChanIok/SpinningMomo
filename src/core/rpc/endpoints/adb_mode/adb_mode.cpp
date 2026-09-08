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

auto handle_list_devices(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> core::rpc::RpcAwaitable<std::vector<features::adb_mode::DiscoveredAdbDevice>> {
  auto result = features::adb_mode::list_devices(app_state);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = result.error(),
    });
  }
  co_return result.value();
}

auto handle_connect_endpoint(core::AppState& app_state,
                             const features::adb_mode::ConnectEndpointParams& params)
    -> core::rpc::RpcAwaitable<features::adb_mode::ConnectEndpointResult> {
  auto result = features::adb_mode::connect_endpoint(app_state, params.host, params.port);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = result.error(),
    });
  }
  co_return result.value();
}

}  // namespace

auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<EmptyParams, features::adb_mode::AdbModeStatus>(
      app_state, app_state.rpc->registry, "adbMode.getStatus", handle_get_status,
      "Get the ADB connection and display status");

  core::rpc::register_method<EmptyParams, std::vector<features::adb_mode::DiscoveredAdbDevice>>(
      app_state, app_state.rpc->registry, "adbMode.listDevices", handle_list_devices,
      "List all available active Android devices and emulators");

  core::rpc::register_method<features::adb_mode::ConnectEndpointParams,
                             features::adb_mode::ConnectEndpointResult>(
      app_state, app_state.rpc->registry, "adbMode.connectEndpoint", handle_connect_endpoint,
      "Manually connect to an Android network endpoint");
}

}  // namespace core::rpc::endpoints::adb_mode
