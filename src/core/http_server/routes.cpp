module;

#include <uwebsockets/App.h>

#include <asio.hpp>

module Core.HttpServer.Routes;

import std;
import Core.State;
import Core.HttpServer.State;
import Core.HttpServer.SseManager;
import Core.HttpServer.Static;
import Core.Async;
import Core.RPC;
import Utils.Logger;

namespace Core::HttpServer::Routes {

auto register_routes(Core::State::AppState& state, uWS::App& app) -> void {
  // 检查状态是否已初始化
  if (!state.http_server) {
    Logger().error("HTTP server not initialized");
    return;
  }

  // 注册RPC端点
  app.post("/rpc", [&state](auto* res, auto* req) {
    std::string buffer;
    res->onData(
        [&state, buffer = std::move(buffer), res](std::string_view data, bool last) mutable {
          buffer.append(data.data(), data.size());

          if (last) {
            // 使用 cork 包裹整个异步操作，延长 res 的生命周期
            res->cork([&state, buffer = std::move(buffer), res]() {
              // 获取事件循环
              auto* loop = uWS::Loop::get();

              // 在异步运行时中处理RPC请求
              asio::co_spawn(
                  *Core::Async::get_io_context(*state.async),
                  [&state, buffer = std::move(buffer), res, loop]() -> asio::awaitable<void> {
                    try {
                      // 处理RPC请求
                      auto response_json = co_await Core::RPC::process_request(state, buffer);

                      // 在事件循环线程中发送响应
                      loop->defer([res, response_json = std::move(response_json)]() {
                        res->writeHeader("Content-Type", "application/json");
                        res->writeStatus("200 OK");
                        res->end(response_json);
                      });
                    } catch (const std::exception& e) {
                      Logger().error("Error processing RPC request: {}", e.what());

                      std::string error_response =
                          std::format(R"({{"error": "Internal server error: {}"}})", e.what());

                      loop->defer([res, error_response = std::move(error_response)]() {
                        res->writeHeader("Content-Type", "application/json");
                        res->writeStatus("500 Internal Server Error");
                        res->end(error_response);
                      });
                    }
                  },
                  asio::detached);
            });
          }
        });

    // 连接中止时记录日志
    res->onAborted([]() { Logger().debug("RPC request aborted"); });
  });

  // 注册SSE端点
  app.get("/sse", [&state](auto* res, auto* req) {
    Logger().info("New SSE connection request");
    Core::HttpServer::SseManager::add_connection(state, res);
  });

  // 配置CORS
  app.options("/*", [](auto* res, auto* req) {
    res->writeHeader("Access-Control-Allow-Origin", "*");
    res->writeHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res->writeHeader("Access-Control-Allow-Headers", "Content-Type");
    res->writeStatus("204 No Content");
    res->end();
  });

  // 静态文件服务（fallback路由）
  Core::HttpServer::Static::register_routes(state, app);
}
}  // namespace Core::HttpServer::Routes