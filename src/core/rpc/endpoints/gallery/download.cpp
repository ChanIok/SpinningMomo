#include "core/rpc/endpoints/gallery/download.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/async/async.hpp"
#include "core/http_server/state.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "core/worker_pool/worker_pool.hpp"
#include "features/gallery/download/download.hpp"
#include "features/gallery/types.hpp"

namespace core::rpc::endpoints::gallery::download {
namespace {

using WorkerResult = std::expected<features::gallery::download::PrepareDownloadResult, std::string>;

struct PrepareDownloadResponse {
  std::string download_url;
  std::string local_download_url;
  std::string file_name;
  std::int64_t failed_count = 0;
};

constexpr std::string_view kDownloadUrlPrefix = "/downloads/";

// 将后台准备结果转换成包含下载地址和用户提示信息的 RPC 响应。
auto make_download_response(const core::AppState& app_state,
                            const features::gallery::download::PrepareDownloadResult& prepared)
    -> std::expected<PrepareDownloadResponse, std::string> {
  // 先保留文件名和失败数量，下面只补充实际可用的下载路径。
  PrepareDownloadResponse response{
      .file_name = prepared.file_name,
      .failed_count = prepared.failed_count,
  };

  // 单文件直接引用资产路由，多文件引用临时归档路由。
  if (prepared.asset_id.has_value()) {
    response.download_url =
        std::format("{}assets/{}", kDownloadUrlPrefix, prepared.asset_id.value());
  } else if (!prepared.archive_token.empty()) {
    response.download_url =
        std::format("{}archives/{}.zip", kDownloadUrlPrefix, prepared.archive_token);
  } else {
    return response;
  }

  // 本地 WebView 需要 HTTP 服务端口；服务未启动时整个响应无法交付。
  if (!app_state.http_server) {
    return std::unexpected("HTTP server state is not available");
  }
  response.local_download_url =
      std::format("http://127.0.0.1:{}{}", app_state.http_server->port, response.download_url);
  return response;
}

// ZIP 准备会等待磁盘读写和 PowerShell 压缩，放到现有 WorkerPool，避免阻塞 RPC 事件循环。
auto prepare_on_worker(core::AppState& app_state, std::vector<std::int64_t> ids)
    -> asio::awaitable<WorkerResult> {
  auto* io_context = core::async::get_io_context(app_state);
  if (!io_context) {
    // 没有异步运行时就无法把 WorkerPool 结果送回 RPC 协程。
    co_return std::unexpected("Async runtime is unavailable");
  }
  if (!core::worker_pool::is_running(app_state)) {
    // 不在 RPC 线程同步执行可能很慢的归档准备。
    co_return std::unexpected("Gallery worker pool is unavailable");
  }

  asio::use_awaitable_t<> completion_token;
  auto result = co_await asio::async_initiate<asio::use_awaitable_t<>, void(WorkerResult)>(
      [&app_state, io_context, ids = std::move(ids)](auto handler) mutable {
        using Handler = decltype(handler);
        // 持有完成处理器，直到后台任务把结果投递回 RPC 运行时。
        auto handler_holder = std::make_shared<Handler>(std::move(handler));

        // 将资产解析和归档创建交给共享 WorkerPool。
        const auto submitted = core::worker_pool::submit_task(
            app_state, [&app_state, io_context, ids = std::move(ids), handler_holder]() mutable {
              WorkerResult result =
                  std::unexpected("Gallery download worker did not return a result");
              try {
                // 在工作线程中执行实际的资产校验和归档准备。
                result = features::gallery::download::prepare(app_state, ids);
              } catch (const std::exception& error) {
                result =
                    std::unexpected(std::string("Gallery download worker failed: ") + error.what());
              } catch (...) {
                result = std::unexpected("Gallery download worker failed with an unknown error");
              }

              // 切回异步运行时，唤醒等待中的 RPC 协程。
              asio::post(*io_context, [handler_holder, result = std::move(result)]() mutable {
                (*handler_holder)(std::move(result));
              });
            });

        if (!submitted) {
          // 提交失败也必须完成 handler，否则调用方会一直等待。
          asio::post(*io_context, [handler_holder]() mutable {
            (*handler_holder)(std::unexpected("Gallery worker pool is unavailable"));
          });
        }
      },
      completion_token);

  co_return result;
}

// 接收图库下载准备结果并转换为 RPC 响应。
auto handle_prepare_download(core::AppState& app_state,
                             const features::gallery::PrepareDownloadParams& params)
    -> RpcAwaitable<PrepareDownloadResponse> {
  auto result = co_await prepare_on_worker(app_state, params.ids);
  if (!result) {
    co_return std::unexpected(
        RpcError{.code = static_cast<int>(ErrorCode::ServerError), .message = result.error()});
  }

  auto response = make_download_response(app_state, result.value());
  if (!response) {
    co_return std::unexpected(
        RpcError{.code = static_cast<int>(ErrorCode::ServerError), .message = response.error()});
  }

  co_return response.value();
}

}  // namespace

auto register_all(core::AppState& app_state) -> void {
  // 仅向已认证的本机或局域网客户端开放下载准备。
  register_method<features::gallery::PrepareDownloadParams, PrepareDownloadResponse>(
      app_state, app_state.rpc->registry, "gallery.prepareDownload", handle_prepare_download,
      "Prepare a direct asset download or a flat ZIP archive for selected gallery assets",
      AccessLevel::lan);
}

}  // namespace core::rpc::endpoints::gallery::download
