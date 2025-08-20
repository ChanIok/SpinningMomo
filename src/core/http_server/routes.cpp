module;

#include <uwebsockets/App.h>

#include <asio.hpp>

module Core.HttpServer.Routes;

import std;
import Core.State;
import Core.HttpServer.State;
import Core.HttpServer.SseManager;
import Core.Async.Runtime;
import Core.RPC.Engine;
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
    Logger().debug("RPC request received");

    // 使用共享指针管理连接状态
    auto connection_state = std::make_shared<std::atomic<bool>>(true);

    std::string buffer;
    res->onData([res, &state, connection_state, buffer = std::move(buffer)](std::string_view data,
                                                                            bool last) mutable {
      buffer.append(data.data(), data.size());

      if (last) {
        // 获取当前的事件循环
        auto* loop = uWS::Loop::get();

        // 在异步运行时中处理RPC请求
        asio::co_spawn(
            *Core::Async::get_io_context(*state.async_runtime),
            [&state, buffer = std::move(buffer), connection_state, loop,
             res]() -> asio::awaitable<void> {
              try {
                // 处理RPC请求
                auto response_json = co_await Core::RPC::process_request(state, buffer);

                // 在事件循环线程中发送响应
                loop->defer([res, connection_state, response_json = std::move(response_json)]() {
                  if (connection_state->load()) {
                    res->writeHeader("Content-Type", "application/json");
                    res->writeStatus("200 OK");
                    res->end(response_json);
                  }
                });
              } catch (const std::exception& e) {
                Logger().error("Error processing RPC request: {}", e.what());

                std::string error_response =
                    std::format(R"({{"error": "Internal server error: {}"}})", e.what());

                loop->defer([res, connection_state, error_response = std::move(error_response)]() {
                  if (connection_state->load()) {
                    res->writeHeader("Content-Type", "application/json");
                    res->writeStatus("500 Internal Server Error");
                    res->end(error_response);
                  }
                });
              }
            },
            asio::detached);
      }
    });

    // 连接中止时标记连接状态为无效
    res->onAborted([connection_state]() {
      connection_state->store(false);
      Logger().debug("RPC request aborted");
    });
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
}
}  // namespace Core::HttpServer::Routes