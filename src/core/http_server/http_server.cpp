module;

#include <uwebsockets/App.h>

module Core.HttpServer;

import std;
import Core.State;
import Core.HttpServer.State;
import Core.HttpServer.Routes;
import Core.HttpServer.SseManager;
import Utils.Logger;

namespace Core::HttpServer {

auto initialize(Core::State::AppState& state) -> std::expected<void, std::string> {
  try {
    Logger().info("Initializing HTTP server on port {}", state.http_server->port);

    state.http_server->server_thread = std::jthread([&state]() {
      Logger().info("Starting HTTP server thread");

      // 在线程中创建uWS::App实例，生命周期由线程管理
      uWS::App app;

      Core::HttpServer::Routes::register_routes(state, app);

      // 启动监听
      app.listen(state.http_server->port, [&state](auto* socket) {
        if (socket) {
          state.http_server->listen_socket = socket;
          state.http_server->is_running = true;
          Logger().info("HTTP server listening on port {}", state.http_server->port);
        } else {
          Logger().error("Failed to start HTTP server on port {}", state.http_server->port);
          state.http_server->is_running = false;
        }
      });

      // 运行事件循环
      if (state.http_server->is_running) {
        state.http_server->loop = uWS::Loop::get();
        app.run();
      }
      Logger().info("HTTP server thread finished");
    });

    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Failed to initialize HTTP server: ") + e.what());
  }
}

auto shutdown(Core::State::AppState& state) -> void {
  if (!state.http_server || !state.http_server->is_running) {
    return;
  }

  Logger().info("Shutting down HTTP server");

  // 使用 defer 将关闭操作调度到事件循环线程
  if (state.http_server->listen_socket) {
    Logger().info("Scheduling socket close");
    state.http_server->loop->defer([listen_socket = state.http_server->listen_socket]() {
      if (listen_socket) {
        us_listen_socket_close(0, listen_socket);
        Logger().info("Listen socket closed");
      }
    });
    state.http_server->listen_socket = nullptr;
    state.http_server->loop = nullptr;
  }

  if (state.http_server->server_thread.joinable()) {
    state.http_server->server_thread.join();
  }

  state.http_server->is_running = false;
  Logger().info("HTTP server shut down");
}

auto get_sse_connection_count(const Core::State::AppState& state) -> size_t {
  return Core::HttpServer::SseManager::get_connection_count(state);
}
}  // namespace Core::HttpServer