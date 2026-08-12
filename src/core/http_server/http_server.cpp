#include "core/http_server/http_server.hpp"

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/build_config.hpp"
#include "core/http_server/access.hpp"
#include "core/http_server/routes.hpp"
#include "core/http_server/sse_manager.hpp"
#include "core/http_server/state.hpp"
#include "core/state/app_state.hpp"
#include "features/settings/state.hpp"
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

// 在 HTTP 线程中关闭旧监听句柄并绑定相同端口的新地址。
auto rebind_listen_socket(core::AppState& state, bool lan_enabled)
    -> std::expected<void, std::string> {
  if (!state.http_server) {
    return std::unexpected("HTTP server state is not allocated");
  }

  auto& server = *state.http_server;
  if (!server.is_running.load()) {
    return std::unexpected("HTTP server is not running");
  }

  auto* loop = server.loop;
  auto* app = server.app;
  auto* old_socket = server.listen_socket;
  if (!loop || !app || !old_socket) {
    return std::unexpected("HTTP server runtime handles are unavailable");
  }

  if (server.runtime_lan_enabled.load() == lan_enabled) {
    return {};
  }

  const auto target_host = lan_enabled ? std::string("0.0.0.0") : std::string("127.0.0.1");
  const auto old_host = server.listen_host;
  const auto port = server.port;
  std::promise<std::expected<void, std::string>> completion;
  auto completion_future = completion.get_future();

  // uWS::App 只能在其所属线程操作；defer() 会把重绑动作投递到 HTTP 事件循环。
  loop->defer([&state, app, old_socket, old_host, target_host, port,
               completion = std::move(completion)]() mutable {
    auto& server = *state.http_server;
    us_listen_socket_close(0, old_socket);
    server.listen_socket = nullptr;

    us_listen_socket_t* new_socket = nullptr;
    app->listen(target_host, port, [&new_socket, &target_host, port](auto* socket) {
      if (!socket) {
        Logger().warn("Failed to rebind HTTP server on {}:{}", target_host, port);
        return;
      }
      new_socket = socket;
    });

    if (new_socket) {
      server.listen_socket = new_socket;
      server.listen_host = target_host;
      server.runtime_lan_enabled = target_host != "127.0.0.1";
      Logger().info("HTTP server rebound to {}:{}", target_host, port);
      completion.set_value({});
      return;
    }

    // 新地址绑定失败时尽量恢复原监听，避免本机 WebView 失去 HTTP 服务。
    us_listen_socket_t* restored_socket = nullptr;
    app->listen(old_host, port, [&restored_socket, &old_host, port](auto* socket) {
      if (!socket) {
        Logger().error("Failed to restore HTTP server on {}:{}", old_host, port);
        return;
      }
      restored_socket = socket;
    });

    if (restored_socket) {
      server.listen_socket = restored_socket;
      server.listen_host = old_host;
      server.runtime_lan_enabled = old_host != "127.0.0.1";
    } else {
      server.runtime_lan_enabled = false;
    }

    completion.set_value(
        std::unexpected(std::format("Failed to rebind HTTP server to {}:{}", target_host, port)));
  });

  return completion_future.get();
}

// 初始化令牌、选择监听范围和端口，并启动 HTTP 事件循环线程。
auto initialize(core::AppState& state) -> std::expected<void, std::string> {
  try {
    if (!state.http_server) {
      return std::unexpected("HTTP server state is not allocated");
    }

    const auto candidate_ports = get_candidate_ports();
    // 令牌必须先准备好，路由注册后才能安全处理远端请求。
    if (auto access_result = core::http_server::access::initialize(state); !access_result) {
      return std::unexpected("Failed to initialize LAN access: " + access_result.error());
    }

    // 根据持久化开关决定只监听回环地址还是监听所有本机接口。
    state.http_server->listen_host =
        state.settings && state.settings->raw.app.lan_access.enabled ? "0.0.0.0" : "127.0.0.1";
    state.http_server->runtime_lan_enabled = false;
    state.http_server->app = nullptr;
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

            // 所有路由都在拥有 uWS App 的线程中注册。
            core::http_server::routes::register_routes(state, app);

            int selected_port = 0;
            us_listen_socket_t* selected_socket = nullptr;

            // 直接尝试绑定，避免“预检查成功后端口又被抢占”的竞态。
            for (const auto port : candidate_ports) {
              Logger().info("Trying HTTP server port {}", port);
              app.listen(state.http_server->listen_host, port,
                         [port, host = state.http_server->listen_host, &selected_port,
                          &selected_socket](auto* socket) {
                           if (!socket) {
                             Logger().warn("Failed to listen on {}:{}", host, port);
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
            state.http_server->loop = uWS::Loop::get();
            state.http_server->app = &app;
            state.http_server->runtime_lan_enabled = state.http_server->listen_host != "127.0.0.1";
            state.http_server->is_running = true;

            Logger().info("HTTP server listening on {}:{}", state.http_server->listen_host,
                          selected_port);
            startup_promise.set_value({});
            startup_reported = true;

            app.run();
            state.http_server->is_running = false;
            state.http_server->runtime_lan_enabled = false;
            state.http_server->app = nullptr;
            auto* loop = state.http_server->loop;
            state.http_server->loop = nullptr;
            if (loop) {
              loop->free();
            }
            Logger().info("HTTP server thread finished");
          } catch (const std::exception& e) {
            state.http_server->is_running = false;
            state.http_server->runtime_lan_enabled = false;
            state.http_server->app = nullptr;
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

// 停止接受请求、关闭 SSE 和监听 socket，并等待 HTTP 线程退出。
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
  state.http_server->app = nullptr;
  state.http_server->loop = nullptr;
  state.http_server->runtime_lan_enabled = false;

  auto remaining_sse = core::http_server::sse_manager::get_connection_count(state);
  Logger().info("Remaining SSE connections after shutdown: {}", remaining_sse);
  Logger().info("HTTP server shut down");
}

// 返回当前 HTTP 服务维护的 SSE 连接数量。
auto get_sse_connection_count(const core::AppState& state) -> size_t {
  return core::http_server::sse_manager::get_connection_count(state);
}
}  // namespace core::http_server
