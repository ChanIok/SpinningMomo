#include "core/http_server/http_server.hpp"

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/build_config.hpp"
#include "core/http_server/routes.hpp"
#include "core/http_server/sse_manager.hpp"
#include "core/http_server/state.hpp"
#include "core/state/app_state.hpp"
#include "utils/logger/logger.hpp"

namespace core::http_server {

namespace {

constexpr std::array kReleaseCandidatePorts{51206, 61206, 11206, 21206, 31206, 41206};

auto get_candidate_ports() -> std::span<const int> {
  if (core::build_config::is_debug_build()) {
    return {kReleaseCandidatePorts.data(), 1};
  }
  return {kReleaseCandidatePorts.data(), kReleaseCandidatePorts.size()};
}

auto format_candidate_ports(std::span<const int> ports) -> std::string {
  std::string result;
  for (const auto port : ports) {
    if (!result.empty()) {
      result += ", ";
    }
    result += std::to_string(port);
  }
  return result;
}

}  // namespace

auto initialize(core::AppState& state) -> std::expected<void, std::string> {
  try {
    if (!state.http_server) {
      return std::unexpected("HTTP server state is not allocated");
    }

    const auto candidate_ports = get_candidate_ports();
    Logger().info("Initializing HTTP server with candidate ports: {}",
                  format_candidate_ports(candidate_ports));

    std::promise<std::expected<void, std::string>> startup_promise;
    auto startup_future = startup_promise.get_future();

    state.http_server->server_thread = std::jthread(
        [&state, candidate_ports, startup_promise = std::move(startup_promise)]() mutable {
          Logger().info("Starting HTTP server thread");
          bool startup_reported = false;

          try {
            // 在线程中创建uWS::App实例，生命周期由线程管理
            uWS::App app;

            core::http_server::routes::register_routes(state, app);

            int selected_port = 0;
            us_listen_socket_t* selected_socket = nullptr;

            // 直接尝试绑定，避免“预检查成功后端口又被抢占”的竞态。
            for (const auto port : candidate_ports) {
              Logger().info("Trying HTTP server port {}", port);
              app.listen("127.0.0.1", port, [port, &selected_port, &selected_socket](auto* socket) {
                if (!socket) {
                  Logger().warn("Failed to listen on 127.0.0.1:{}", port);
                  return;
                }

                selected_port = port;
                selected_socket = socket;
              });

              if (selected_socket) {
                break;
              }
            }

            if (!selected_socket) {
              auto error = std::format("Failed to listen on candidate ports: {}",
                                       format_candidate_ports(candidate_ports));
              Logger().error(error);
              startup_promise.set_value(std::unexpected(error));
              startup_reported = true;
              Logger().info("HTTP server thread finished");
              return;
            }

            state.http_server->port = selected_port;
            state.http_server->listen_socket = selected_socket;
            state.http_server->is_running = true;
            state.http_server->loop = uWS::Loop::get();

            Logger().info("HTTP server listening on 127.0.0.1:{}", selected_port);
            startup_promise.set_value({});
            startup_reported = true;

            app.run();
            state.http_server->loop->free();
            state.http_server->loop = nullptr;
            Logger().info("HTTP server thread finished");
          } catch (const std::exception& e) {
            state.http_server->is_running = false;
            auto error = std::string("HTTP server thread failed: ") + e.what();
            Logger().error(error);
            if (!startup_reported) {
              startup_promise.set_value(std::unexpected(error));
            }
          }
        });

    auto startup_result = startup_future.get();
    if (!startup_result) {
      if (state.http_server->server_thread.joinable()) {
        state.http_server->server_thread.join();
      }
      return std::unexpected(startup_result.error());
    }

    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Failed to initialize HTTP server: ") + e.what());
  }
}

auto shutdown(core::AppState& state) -> void {
  if (!state.http_server || !state.http_server->is_running) {
    return;
  }

  Logger().info("Shutting down HTTP server");

  auto active_sse = core::http_server::sse_manager::get_connection_count(state);
  Logger().info("Active SSE connections before shutdown: {}", active_sse);

  // 提前标记停止，避免 shutdown 过程中继续广播 SSE 事件
  state.http_server->is_running = false;

  auto* loop = state.http_server->loop;
  auto* listen_socket = state.http_server->listen_socket;

  // 使用 defer 将关闭操作调度到事件循环线程
  if (loop) {
    Logger().info("Scheduling SSE close and socket close");
    loop->defer([&state, listen_socket]() {
      core::http_server::sse_manager::close_all_connections(state);

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

  auto remaining_sse = core::http_server::sse_manager::get_connection_count(state);
  Logger().info("Remaining SSE connections after shutdown: {}", remaining_sse);
  Logger().info("HTTP server shut down");
}

auto get_sse_connection_count(const core::AppState& state) -> size_t {
  return core::http_server::sse_manager::get_connection_count(state);
}
}  // namespace core::http_server
