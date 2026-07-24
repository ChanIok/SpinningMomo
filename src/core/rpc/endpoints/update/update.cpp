#include "core/rpc/endpoints/update/update.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/rfl.hpp"

#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "features/update/types.hpp"
#include "features/update/update.hpp"

namespace core::rpc::endpoints::update {

auto handle_check_for_update(core::AppState& app_state, [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<core::rpc::RpcResult<features::update::CheckUpdateResult>> {
  auto result = co_await features::update::check_for_update(app_state);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_start_download(core::AppState& app_state, [[maybe_unused]] const rfl::Generic& params)
    -> asio::awaitable<core::rpc::RpcResult<features::update::StartDownloadUpdateResult>> {
  auto result = co_await features::update::start_download_update_task(app_state);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_install_update(core::AppState& app_state,
                           const features::update::InstallUpdateParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::update::InstallUpdateResult>> {
  auto result = features::update::install_update(app_state, params);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<rfl::Generic, features::update::CheckUpdateResult>(
      app_state, app_state.rpc->registry, "update.check_for_update", handle_check_for_update,
      "Check for available updates");

  core::rpc::register_method<rfl::Generic, features::update::StartDownloadUpdateResult>(
      app_state, app_state.rpc->registry, "update.start_download", handle_start_download,
      "Start downloading update package in background");

  core::rpc::register_method<features::update::InstallUpdateParams,
                             features::update::InstallUpdateResult>(
      app_state, app_state.rpc->registry, "update.install_update", handle_install_update,
      "Install downloaded update");
}

}  // namespace core::rpc::endpoints::update
