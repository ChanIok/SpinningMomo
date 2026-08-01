#include "core/http_server/routes.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/uwebsockets.hpp"

#include "core/async/async.hpp"
#include "core/build_config.hpp"
#include "core/http_server/sse_manager.hpp"
#include "core/http_server/state.hpp"
#include "core/http_server/static.hpp"
#include "core/rpc/rpc.hpp"
#include "core/state/app_state.hpp"
#include "utils/logger/logger.hpp"

namespace core::http_server::routes {

auto get_origin_header(auto* req) -> std::string { return std::string(req->getHeader("origin")); }

auto is_local_origin_allowed(std::string_view origin, int port) -> bool {
  const auto localhost = std::format("http://localhost:{}", port);
  const auto loopback_v4 = std::format("http://127.0.0.1:{}", port);
  const auto loopback_v6 = std::format("http://[::1]:{}", port);

  return origin == localhost || origin == loopback_v4 || origin == loopback_v6;
}

auto is_origin_allowed(std::string_view origin, int port) -> bool {
  if (origin.empty()) {
    // 无 Origin 通常来自非浏览器本地请求。
    return true;
  }

  // 开发模式放行所有 Origin，便于局域网/多设备联调。
  if (core::build_config::is_debug_build()) {
    return true;
  }

  // 发布模式仅允许本机同端口来源。
  return is_local_origin_allowed(origin, port);
}

auto write_cors_headers(auto* res, std::string_view origin) -> void {
  if (!origin.empty()) {
    res->writeHeader("Access-Control-Allow-Origin", origin);
    res->writeHeader("Vary", "Origin");
  }
  res->writeHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res->writeHeader("Access-Control-Allow-Headers", "Content-Type");
}

auto reject_forbidden(auto* res) -> void {
  res->writeStatus("403 Forbidden");
  res->end("Forbidden");
}

auto register_routes(core::AppState& state, uWS::App& app) -> void {
  // 检查状态是否已初始化
  if (!state.http_server) {
    Logger().error("HTTP server not initialized");
    return;
  }

  // 注册rpc端点
  app.post("/rpc", [&state](auto* res, auto* req) {
    auto origin = get_origin_header(req);
    if (!is_origin_allowed(origin, state.http_server->port)) {
      Logger().warn("Rejected RPC request due to disallowed origin: {}",
                    origin.empty() ? "<empty>" : origin);
      reject_forbidden(res);
      return;
    }

    std::string buffer;
    res->onData([&state, buffer = std::move(buffer), origin = std::move(origin), res](
                    std::string_view data, bool last) mutable {
      buffer.append(data.data(), data.size());

      if (last) {
        // 使用 cork 包裹整个异步操作，延长 res 的生命周期
        res->cork([&state, buffer = std::move(buffer), origin = std::move(origin), res]() {
          // 获取事件循环
          auto* loop = uWS::Loop::get();

          // 在异步运行时中处理rpc请求
          asio::co_spawn(
              *core::async::get_io_context(state),
              [&state, buffer = std::move(buffer), origin = std::move(origin), res,
               loop]() -> asio::awaitable<void> {
                try {
                  // 处理rpc请求
                  auto response_json = co_await core::rpc::process_request(state, buffer);

                  // 在事件循环线程中发送响应
                  loop->defer([res, origin, response_json = std::move(response_json)]() {
                    write_cors_headers(res, origin);
                    res->writeHeader("Content-Type", "application/json");
                    res->writeStatus("200 OK");
                    res->end(response_json);
                  });
                } catch (const std::exception& e) {
                  Logger().error("Error processing RPC request: {}", e.what());

                  std::string error_response =
                      std::format(R"({{"error": "Internal server error: {}"}})", e.what());

                  loop->defer([res, origin, error_response = std::move(error_response)]() {
                    write_cors_headers(res, origin);
                    res->writeHeader("Content-Type", "application/json");
                    res->writeStatus("500 Internal Server Error");
                    res->end(error_response);
                  });
                }
              },
              core::async::log_completion("HTTP RPC request"));
        });
      }
    });

    // 连接中止时记录日志
    res->onAborted([]() { Logger().debug("RPC request aborted"); });
  });

  // 注册SSE端点
  app.get("/sse", [&state](auto* res, auto* req) {
    if (!state.http_server || !state.http_server->is_running) {
      res->close();
      return;
    }

    auto origin = get_origin_header(req);
    if (!is_origin_allowed(origin, state.http_server->port)) {
      Logger().warn("Rejected SSE request due to disallowed origin: {}",
                    origin.empty() ? "<empty>" : origin);
      reject_forbidden(res);
      return;
    }

    Logger().info("New SSE connection request");
    core::http_server::sse_manager::add_connection(state, res, std::move(origin));
  });

  // 配置CORS
  app.options("/*", [&state](auto* res, auto* req) {
    auto origin = get_origin_header(req);
    if (!is_origin_allowed(origin, state.http_server->port)) {
      reject_forbidden(res);
      return;
    }

    write_cors_headers(res, origin);
    res->writeStatus("204 No Content");
    res->end();
  });

  // 静态文件服务（fallback路由）
  core::http_server::static_content::register_routes(state, app);
}
}  // namespace core::http_server::routes
