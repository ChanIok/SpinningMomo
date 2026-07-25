#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

namespace utils::network {

struct TcpProbeResult {
  bool reachable = false;
  int error_code = 0;
  std::string reason;
};

// 短超时 TCP 端口探测，只证明端口层可能可达。
// 不触碰 UNC 文件系统路径，也不证明 SMB share、权限或 WebView2 映射一定可用。
auto probe_tcp_port(const std::wstring& server, const std::wstring& port,
                    std::chrono::milliseconds timeout) -> asio::awaitable<TcpProbeResult>;

}  // namespace utils::network
