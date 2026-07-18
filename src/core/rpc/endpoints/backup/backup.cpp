module;

module Core.RPC.Endpoints.Backup;

import std;
import Core.Async;
import Core.Events;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.State;
import Features.Backup;
import Features.Backup.Types;
import UI.FloatingWindow.Events;
import <asio.hpp>;

namespace Core::RPC::Endpoints::Backup {

// 将业务错误统一映射为 JSON-RPC 服务错误。
auto make_service_error(std::string error) -> Core::RPC::RpcError {
  return Core::RPC::RpcError{
      .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
      .message = "Service error: " + std::move(error),
  };
}

// 导出当前用户数据到调用方选择的目录。
auto handle_export(Core::State::AppState& app_state,
                   const Features::Backup::Types::ExportParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Backup::Types::ExportResult>> {
  auto result = Features::Backup::export_backup(app_state, params);
  if (!result) {
    co_return std::unexpected(make_service_error(result.error()));
  }
  co_return std::move(*result);
}

// 启动完全替换恢复脚本，并在响应送达前端后请求应用退出。
auto handle_restore(Core::State::AppState& app_state,
                    const Features::Backup::Types::RestoreParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Backup::Types::RestoreResult>> {
  auto result = Features::Backup::restore_backup(params);
  if (!result) {
    co_return std::unexpected(make_service_error(result.error()));
  }

  auto* io_context = Core::Async::get_io_context(app_state);
  if (io_context) {
    asio::co_spawn(
        *io_context,
        [&app_state]() -> asio::awaitable<void> {
          // 给 RPC 桥留出发送成功响应的时间，再由 UI 线程执行完整退出流程。
          asio::steady_timer timer(co_await asio::this_coro::executor,
                                   std::chrono::milliseconds(750));
          co_await timer.async_wait(asio::use_awaitable);
          Core::Events::post(app_state, UI::FloatingWindow::Events::ExitEvent{});
        },
        asio::detached_t{});
  }

  co_return std::move(*result);
}

// 注册数据导出和完全替换恢复端点。
auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<Features::Backup::Types::ExportParams,
                             Features::Backup::Types::ExportResult>(
      app_state, app_state.rpc->registry, "backup.export", handle_export,
      "Export database, settings, managed backgrounds and app version to ZIP");

  Core::RPC::register_method<Features::Backup::Types::RestoreParams,
                             Features::Backup::Types::RestoreResult>(
      app_state, app_state.rpc->registry, "backup.restore", handle_restore,
      "Replace application data from ZIP after exit and restart the application");
}

}  // namespace Core::RPC::Endpoints::Backup
