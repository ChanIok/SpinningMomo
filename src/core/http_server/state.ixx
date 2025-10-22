module;

#include <uwebsockets/App.h>

export module Core.HttpServer.State;

import std;
import Core.HttpServer.Types;

export namespace Core::HttpServer::State {

// HTTP服务器状态
struct HttpServerState {
  // 服务器核心
  std::jthread server_thread{};
  us_listen_socket_t* listen_socket{nullptr};
  uWS::Loop* loop{nullptr};

  // SSE连接管理
  std::vector<std::shared_ptr<Types::SseConnection>> sse_connections;
  std::atomic<std::uint64_t> client_counter{0};
  std::mutex sse_connections_mutex;
  std::atomic<bool> is_running{false};

  // 服务器配置
  int port{51206};

  // 路径解析器注册表
  Types::ResolverRegistry path_resolvers;
};

}  // namespace Core::HttpServer::State