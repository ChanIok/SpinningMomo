#include "core/rpc/endpoints/backup/backup.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/async/async.hpp"
#include "core/events/events.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "features/backup/backup.hpp"
#include "features/backup/types.hpp"
#include "ui/floating_window/events.hpp"

namespace core::rpc::endpoints::backup {

// 将业务错误统一映射为 JSON-RPC 服务错误。
auto make_service_error(std::string error) -> core::rpc::RpcError {
  return core::rpc::RpcError{
      .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
      .message = "Service error: " + std::move(error),
  };
}

// 导出当前用户数据到调用方选择的目录。
auto handle_export(core::AppState& app_state, const features::backup::ExportParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::backup::ExportResult>> {
  auto result = features::backup::export_backup(app_state, params);
  if (!result) {
    co_return std::unexpected(make_service_error(result.error()));
  }
  co_return std::move(*result);
}

// 启动完全替换恢复脚本，并在响应送达前端后请求应用退出。
auto handle_restore(core::AppState& app_state, const features::backup::RestoreParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::backup::RestoreResult>> {
  auto result = features::backup::restore_backup(params);
  if (!result) {
    co_return std::unexpected(make_service_error(result.error()));
  }

  auto* io_context = core::async::get_io_context(app_state);
  if (io_context) {
    asio::co_spawn(
        *io_context,
        [&app_state]() -> asio::awaitable<void> {
          // 给 RPC 桥留出发送成功响应的时间，再由 UI 线程执行完整退出流程。
          asio::steady_timer timer(co_await asio::this_coro::executor,
                                   std::chrono::milliseconds(750));
          co_await timer.async_wait(asio::use_awaitable);
          core::events::post(app_state, ui::floating_window::events::ExitEvent{});
        },
        asio::detached_t{});
  }

  co_return std::move(*result);
}

// 注册数据导出和完全替换恢复端点。
auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<features::backup::ExportParams, features::backup::ExportResult>(
      app_state, app_state.rpc->registry, "backup.export", handle_export,
      "Export database, settings, managed backgrounds and app version to ZIP");

  core::rpc::register_method<features::backup::RestoreParams, features::backup::RestoreResult>(
      app_state, app_state.rpc->registry, "backup.restore", handle_restore,
      "Replace application data from ZIP after exit and restart the application");
}

}  // namespace core::rpc::endpoints::backup
