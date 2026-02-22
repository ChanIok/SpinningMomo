module;

#include <uwebsockets/App.h>

module Core.HttpServer.SseManager;

import std;
import Core.State;
import Core.HttpServer.State;
import Core.HttpServer.Types;
import Utils.Logger;

namespace Core::HttpServer::SseManager {

auto format_sse_message(const std::string& event_data) -> std::string {
  return std::format("data: {}\n\n", event_data);
}

auto add_connection(Core::State::AppState& state, uWS::HttpResponse<false>* response,
                    std::string allowed_origin) -> void {
  if (!state.http_server || !response) {
    Logger().error("Cannot add SSE connection: invalid state or response");
    return;
  }

  auto& connections = state.http_server->sse_connections;
  auto& counter = state.http_server->client_counter;
  auto& mtx = state.http_server->sse_connections_mutex;

  auto connection = std::make_shared<Types::SseConnection>();
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

auto remove_connection(Core::State::AppState& state, const std::string& client_id) -> void {
  if (!state.http_server) {
    return;
  }

  auto& connections = state.http_server->sse_connections;
  auto& mtx = state.http_server->sse_connections_mutex;

  std::lock_guard<std::mutex> lock(mtx);

  auto old_size = connections.size();
  auto it = std::remove_if(connections.begin(), connections.end(),
                           [&client_id](const std::shared_ptr<Types::SseConnection>& conn) {
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

auto close_all_connections(Core::State::AppState& state) -> void {
  if (!state.http_server) {
    return;
  }

  auto& connections = state.http_server->sse_connections;
  auto& mtx = state.http_server->sse_connections_mutex;

  std::vector<std::shared_ptr<Types::SseConnection>> snapshot;
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
    conn->response->end();
    ++closed_count;
  }

  Logger().info("Closed {} SSE connections during shutdown", closed_count);
}

auto broadcast_event(Core::State::AppState& state, const std::string& event_data) -> void {
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

    std::vector<std::shared_ptr<Types::SseConnection>> snapshot;
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

auto get_connection_count(const Core::State::AppState& state) -> size_t {
  if (!state.http_server) {
    return 0;
  }

  auto& connections = state.http_server->sse_connections;
  auto& mtx = state.http_server->sse_connections_mutex;

  std::lock_guard<std::mutex> lock(mtx);
  return connections.size();
}
}  // namespace Core::HttpServer::SseManager
