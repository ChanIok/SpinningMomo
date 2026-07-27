#include "core/http_server/sse_manager.hpp"

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/http_server/state.hpp"
#include "core/http_server/types.hpp"
#include "core/state/app_state.hpp"
#include "utils/logger/logger.hpp"

namespace core::http_server::sse_manager {

auto format_sse_message(const std::string& event_data) -> std::string {
  return std::format("data: {}\n\n", event_data);
}

auto add_connection(core::AppState& state, uWS::HttpResponse<false>* response,
                    std::string allowed_origin) -> void {
  if (!state.http_server || !response) {
    Logger().error("Cannot add SSE connection: invalid state or response");
    return;
  }

  auto& connections = state.http_server->sse_connections;
  auto& counter = state.http_server->client_counter;
  auto& mtx = state.http_server->sse_connections_mutex;

  auto connection = std::make_shared<SseConnection>();
  connection->response = response;
  connection->client_id = std::to_string(++counter);
  connection->connected_at = std::chrono::system_clock::now();

  response->onAborted(
      [&state, client_id = connection->client_id]() { remove_connection(state, client_id); });

  response->writeStatus("200 OK");
  response->writeHeader("Content-Type", "text/event-stream");
  response->writeHeader("Cache-Control", "no-cache");
  response->writeHeader("Connection", "keep-alive");
  if (!allowed_origin.empty()) {
    response->writeHeader("Access-Control-Allow-Origin", allowed_origin);
    response->writeHeader("Vary", "Origin");
  }
  response->write(": connected\n\n");

  size_t current_count = 0;
  {
    std::lock_guard<std::mutex> lock(mtx);
    connections.push_back(connection);
    current_count = connections.size();
  }

  Logger().info("New SSE connection established. client_id={}, total={}", connection->client_id,
                current_count);
}

auto remove_connection(core::AppState& state, const std::string& client_id) -> void {
  if (!state.http_server) {
    return;
  }

  auto& connections = state.http_server->sse_connections;
  auto& mtx = state.http_server->sse_connections_mutex;

  std::lock_guard<std::mutex> lock(mtx);

  auto old_size = connections.size();
  auto it = std::remove_if(connections.begin(), connections.end(),
                           [&client_id](const std::shared_ptr<SseConnection>& conn) {
                             if (conn && conn->client_id == client_id) {
                               conn->is_closed = true;
                               return true;
                             }
                             return false;
                           });
  connections.erase(it, connections.end());

  if (connections.size() < old_size) {
    Logger().info("SSE connection removed. client_id={}, total={}", client_id, connections.size());
  }
}

auto close_all_connections(core::AppState& state) -> void {
  if (!state.http_server) {
    return;
  }

  auto& connections = state.http_server->sse_connections;
  auto& mtx = state.http_server->sse_connections_mutex;

  std::vector<std::shared_ptr<SseConnection>> snapshot;
  {
    std::lock_guard<std::mutex> lock(mtx);
    snapshot.reserve(connections.size());
    for (const auto& conn : connections) {
      if (!conn) {
        continue;
      }
      conn->is_closed = true;
      snapshot.push_back(conn);
    }
    connections.clear();
  }

  size_t closed_count = 0;
  for (const auto& conn : snapshot) {
    if (!conn || !conn->response) {
      continue;
    }
    // end() 只结束当前 SSE 响应，浏览器仍可复用 keep-alive 连接发起重连。
    // shutdown 必须关闭底层连接，否则 uWS 事件循环不会退出，server_thread::join 会永久等待。
    conn->response->close();
    ++closed_count;
  }

  Logger().info("Closed {} SSE connections during shutdown", closed_count);
}

auto broadcast_event(core::AppState& state, const std::string& event_data) -> void {
  if (!state.http_server || !state.http_server->is_running) {
    return;
  }

  auto* loop = state.http_server->loop;
  if (!loop) {
    return;
  }

  auto sse_message = format_sse_message(event_data);

  loop->defer([&state, sse_message = std::move(sse_message)]() {
    if (!state.http_server) {
      return;
    }

    auto& connections = state.http_server->sse_connections;
    auto& mtx = state.http_server->sse_connections_mutex;

    std::vector<std::shared_ptr<SseConnection>> snapshot;
    {
      std::lock_guard<std::mutex> lock(mtx);
      snapshot.reserve(connections.size());
      for (const auto& conn : connections) {
        if (conn && !conn->is_closed) {
          snapshot.push_back(conn);
        }
      }
    }

    if (snapshot.empty()) {
      return;
    }

    for (const auto& conn : snapshot) {
      if (!conn || !conn->response || conn->is_closed) {
        continue;
      }
      const auto ok = conn->response->write(sse_message);
      if (!ok) {
        Logger().warn("SSE write reported backpressure for client {}", conn->client_id);
      }
    }
  });
}

auto get_connection_count(const core::AppState& state) -> size_t {
  if (!state.http_server) {
    return 0;
  }

  auto& connections = state.http_server->sse_connections;
  auto& mtx = state.http_server->sse_connections_mutex;

  std::lock_guard<std::mutex> lock(mtx);
  return connections.size();
}
}  // namespace core::http_server::sse_manager
