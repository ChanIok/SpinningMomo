#include "core/http_server/routes.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/uwebsockets.hpp"

#include "core/async/async.hpp"
#include "core/build_config.hpp"
#include "core/http_server/access.hpp"
#include "core/http_server/downloads.hpp"
#include "core/http_server/sse_manager.hpp"
#include "core/http_server/state.hpp"
#include "core/http_server/static.hpp"
#include "core/rpc/rpc.hpp"
#include "core/state/app_state.hpp"
#include "utils/logger/logger.hpp"

namespace core::http_server::routes {

// 从 uWebSockets 请求对象读取 Origin，供 CORS 和同源校验复用。
auto get_origin_header(auto* req) -> std::string { return std::string(req->getHeader("origin")); }

// 判断 Origin 是否来自本机同端口页面。
auto is_local_origin_allowed(std::string_view origin, int port) -> bool {
  const auto localhost = std::format("http://localhost:{}", port);
  const auto loopback_v4 = std::format("http://127.0.0.1:{}", port);
  const auto loopback_v6 = std::format("http://[::1]:{}", port);

  return origin == localhost || origin == loopback_v4 || origin == loopback_v6;
}

// 按构建模式和当前 Host 判断请求来源是否允许继续处理。
auto is_origin_allowed(std::string_view origin, int port, std::string_view host) -> bool {
  if (origin.empty()) {
    // 无 Origin 通常来自非浏览器本地请求。
    return true;
  }

  // 开发模式放行所有 Origin，便于局域网/多设备联调。
  if (core::build_config::is_debug_build()) {
    return true;
  }

  // 发布模式允许本机同端口来源，以及经过鉴权后从当前 HTTP Host 加载的同源页面。
  if (is_local_origin_allowed(origin, port)) {
    return true;
  }
  return origin == std::format("http://{}", host);
}

// 为允许的跨域请求写出统一的预检和响应头。
auto write_cors_headers(auto* res, std::string_view origin) -> void {
  if (!origin.empty()) {
    res->writeHeader("Access-Control-Allow-Origin", origin);
    res->writeHeader("Vary", "Origin");
  }
  res->writeHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res->writeHeader("Access-Control-Allow-Headers", "Content-Type");
}

// 返回来源不被允许的 HTTP 403 响应。
auto reject_forbidden(auto* res) -> void {
  res->writeStatus("403 Forbidden");
  res->end("Forbidden");
}

// 返回缺少有效本地或 LAN 会话的 HTTP 401 响应。
auto reject_unauthorized(auto* res) -> void {
  // 先写状态码，避免 uWebSockets 在首个响应头处固定默认的 200。
  res->writeStatus("401 Unauthorized");
  res->writeHeader("Cache-Control", "no-store");
  res->writeHeader("Content-Type", "text/plain; charset=utf-8");
  res->end("Authentication required");
}

// 将请求的远端地址和 Cookie 转换成 RPC 需要的访问等级。
auto resolve_request_access(core::AppState& state, auto* res, auto* req)
    -> std::optional<core::rpc::AccessLevel> {
  return core::http_server::access::resolve_http_access(state, res->getRemoteAddressAsText(),
                                                        req->getHeader("cookie"));
}

// 用短链接中的令牌建立 LAN 会话，再跳转到不携带凭据的应用首页。
auto handle_token_exchange(core::AppState& state, auto* res, auto* req) -> void {
  const std::string_view token = req->getParameter("token");

  if (!core::http_server::access::is_remote_access_enabled(state)) {
    res->writeStatus("403 Forbidden");
    res->writeHeader("Cache-Control", "no-store");
    res->writeHeader("Content-Type", "text/html; charset=utf-8");
    res->end("<h1>LAN access is unavailable</h1><p>Enable LAN access in SpinningMomo.</p>");
    return;
  }

  if (!core::http_server::access::is_token_valid(state, token)) {
    res->writeStatus("401 Unauthorized");
    res->writeHeader("Cache-Control", "no-store");
    res->writeHeader("Content-Type", "text/html; charset=utf-8");
    res->end("<h1>Invalid access link</h1><p>Generate a new link in SpinningMomo.</p>");
    return;
  }

  res->writeStatus("302 Found");
  res->writeHeader("Set-Cookie", core::http_server::access::make_cookie(token));
  res->writeHeader("Location", "/#/");
  res->writeHeader("Cache-Control", "no-store");
  res->writeHeader("Referrer-Policy", "no-referrer");
  res->end();
}

// 注册令牌交换、RPC、SSE、CORS 和静态资源路由。
auto register_routes(core::AppState& state, uWS::App& app) -> void {
  // 检查状态是否已初始化
  if (!state.http_server) {
    Logger().error("HTTP server not initialized");
    return;
  }

  // 令牌只在低频的会话建立路由中解析，普通静态资源请求不承担这项工作。
  app.get("/t/:token", [&state](auto* res, auto* req) { handle_token_exchange(state, res, req); });

  // 注册rpc端点
  app.post("/rpc", [&state](auto* res, auto* req) {
    // HTTP 层先完成身份认证，再把访问等级传入 JSON-RPC 层。
    const auto caller_access = resolve_request_access(state, res, req);
    if (!caller_access) {
      reject_unauthorized(res);
      return;
    }

    // 身份通过后再校验 Origin，阻止其他站点借用已登录 Cookie。
    auto origin = get_origin_header(req);
    const auto host = std::string(req->getHeader("host"));
    if (!is_origin_allowed(origin, state.http_server->port, host)) {
      Logger().warn("Rejected RPC request due to disallowed origin: {}",
                    origin.empty() ? "<empty>" : origin);
      reject_forbidden(res);
      return;
    }

    std::string buffer;
    res->onData([&state, caller_access = *caller_access, buffer = std::move(buffer),
                 origin = std::move(origin), res](std::string_view data, bool last) mutable {
      buffer.append(data.data(), data.size());

      if (last) {
        // 收齐请求体后才进入异步 RPC 流程，避免分片数据被提前解析。
        // 使用 cork 包裹整个异步操作，延长 res 的生命周期
        res->cork(
            [&state, caller_access, buffer = std::move(buffer), origin = std::move(origin), res]() {
              // 获取事件循环
              auto* loop = uWS::Loop::get();

              // 在异步运行时中处理rpc请求
              asio::co_spawn(
                  *core::async::get_io_context(state),
                  [&state, caller_access, buffer = std::move(buffer), origin = std::move(origin),
                   res, loop]() -> asio::awaitable<void> {
                    try {
                      // 处理rpc请求
                      // 将 HTTP 层确认的访问等级贯穿到每个 RPC 方法。
                      auto response_json =
                          co_await core::rpc::process_request(state, buffer, caller_access);

                      // 在事件循环线程中发送响应
                      loop->defer([res, origin, response_json = std::move(response_json)]() {
                        // 先确定状态，再写 CORS 和内容头。
                        res->writeStatus("200 OK");
                        write_cors_headers(res, origin);
                        res->writeHeader("Content-Type", "application/json");
                        res->end(response_json);
                      });
                    } catch (const std::exception& e) {
                      Logger().error("Error processing RPC request: {}", e.what());

                      std::string error_response =
                          std::format(R"({{"error": "Internal server error: {}"}})", e.what());

                      loop->defer([res, origin, error_response = std::move(error_response)]() {
                        // 异常响应同样先写状态，避免被默认 200 覆盖。
                        res->writeStatus("500 Internal Server Error");
                        write_cors_headers(res, origin);
                        res->writeHeader("Content-Type", "application/json");
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

    // SSE 与普通 RPC 使用同一套会话认证，避免只保护写请求。
    const auto caller_access = resolve_request_access(state, res, req);
    if (!caller_access) {
      reject_unauthorized(res);
      return;
    }

    auto origin = get_origin_header(req);
    const auto host = std::string(req->getHeader("host"));
    if (!is_origin_allowed(origin, state.http_server->port, host)) {
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
    // 预检请求也必须先认证，不能成为绕过访问边界的入口。
    const auto caller_access = resolve_request_access(state, res, req);
    if (!caller_access) {
      reject_unauthorized(res);
      return;
    }

    auto origin = get_origin_header(req);
    const auto host = std::string(req->getHeader("host"));
    if (!is_origin_allowed(origin, state.http_server->port, host)) {
      reject_forbidden(res);
      return;
    }

    res->writeStatus("204 No Content");
    write_cors_headers(res, origin);
    res->end();
  });

  // 下载路由必须在静态 fallback 前注册，避免临时归档被当作前端资源处理。
  core::http_server::downloads::register_routes(state, app);

  // 静态文件服务（fallback路由）
  core::http_server::static_content::register_routes(state, app);
}
}  // namespace core::http_server::routes
