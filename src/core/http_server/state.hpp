#pragma once

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/http_server/types.hpp"

namespace core::http_server {

// HTTP服务器状态
struct HttpServerState {
  // 服务器核心
  std::jthread server_thread{};
  // 非拥有指针；指向 server_thread 栈上的 uWS::App，只能在 HTTP loop 的 defer 回调中使用。
  uWS::App* app{nullptr};
  // 由 HTTP 服务创建、并在 shutdown/rebind 中显式关闭的监听句柄。
  us_listen_socket_t* listen_socket{nullptr};
  // 非拥有事件循环指针；跨线程只用于投递 defer() 回调。
  uWS::Loop* loop{nullptr};

  // SSE连接管理
  std::vector<std::shared_ptr<SseConnection>> sse_connections;
  std::atomic<std::uint64_t> client_counter{0};
  std::mutex sse_connections_mutex;
  std::atomic<bool> is_running{false};
  std::atomic<bool> runtime_lan_enabled{false};

  // 服务器配置；启动成功后更新为实际监听端口
  int port{51206};
  std::string listen_host{"127.0.0.1"};

  // 局域网访问令牌；只保存在运行时状态和独立的应用数据文件中，不进入 AppSettings。
  std::string access_token;
  mutable std::shared_mutex access_token_mutex;

  // 路径解析器注册表
  ResolverRegistry path_resolvers;
};

}  // namespace core::http_server
