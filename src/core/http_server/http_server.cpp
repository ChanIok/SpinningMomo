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

      // 仅监听本机回环地址，避免暴露到局域网
      app.listen("127.0.0.1", state.http_server->port, [&state](auto* socket) {
        if (socket) {
          state.http_server->listen_socket = socket;
          state.http_server->is_running = true;
          Logger().info("HTTP server listening on 127.0.0.1:{}", state.http_server->port);
        } else {
          Logger().error("Failed to start HTTP server on 127.0.0.1:{}", state.http_server->port);
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

  auto active_sse = Core::HttpServer::SseManager::get_connection_count(state);
  Logger().info("Active SSE connections before shutdown: {}", active_sse);

  // 提前标记停止，避免 shutdown 过程中继续广播 SSE 事件
  state.http_server->is_running = false;

  auto* loop = state.http_server->loop;
  auto* listen_socket = state.http_server->listen_socket;

  // 使用 defer 将关闭操作调度到事件循环线程
  if (loop) {
    Logger().info("Scheduling SSE close and socket close");
    loop->defer([&state, listen_socket]() {
      Core::HttpServer::SseManager::close_all_connections(state);

      if (listen_socket) {
        us_listen_socket_close(0, listen_socket);
        Logger().info("Listen socket closed");
      }
    });
  } else {
    Logger().warn("HTTP loop is null during shutdown; listen socket close was not scheduled");
  }

  if (state.http_server->server_thread.joinable()) {
    state.http_server->server_thread.join();
  }

  state.http_server->listen_socket = nullptr;
  state.http_server->loop = nullptr;

  auto remaining_sse = Core::HttpServer::SseManager::get_connection_count(state);
  Logger().info("Remaining SSE connections after shutdown: {}", remaining_sse);
  Logger().info("HTTP server shut down");
}

auto get_sse_connection_count(const Core::State::AppState& state) -> size_t {
  return Core::HttpServer::SseManager::get_connection_count(state);
}
}  // namespace Core::HttpServer
