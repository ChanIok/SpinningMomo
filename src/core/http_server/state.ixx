module;

#include <uwebsockets/App.h>

export module Core.HttpServer.State;

import std;

namespace Core::HttpServer::State {

// SSE连接信息结构
export struct SseConnection {
  uWS::HttpResponse<false>* response = nullptr;
  std::string client_id;
  std::chrono::system_clock::time_point connected_at;
  bool is_closed = false;
};

// HTTP服务器状态
export struct HttpServerState {
  // 服务器核心
  std::jthread server_thread{};
  us_listen_socket_t* listen_socket{nullptr};
  uWS::Loop* loop{nullptr};

  // SSE连接管理
  std::vector<std::shared_ptr<SseConnection>> sse_connections;
  std::atomic<std::uint64_t> client_counter{0};
  std::mutex sse_connections_mutex;
  std::atomic<bool> is_running{false};

  // 服务器配置
  int port{51205};
};
}  // namespace Core::HttpServer::State