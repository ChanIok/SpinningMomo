#pragma once

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/http_server/types.hpp"

namespace core::http_server {

// HTTP服务器状态
struct HttpServerState {
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
  int port{51206};

  // 路径解析器注册表
  ResolverRegistry path_resolvers;
};

}  // namespace core::http_server
