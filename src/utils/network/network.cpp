module;

module Utils.Network;

import std;
import Utils.String;
import <asio.hpp>;

namespace Utils::Network::Detail {

auto reason_from_error(const asio::error_code& error) -> std::string {
  if (!error) {
    return {};
  }

  if (error == asio::error::operation_aborted) {
    return "TCP probe was cancelled";
  }
  if (error == asio::error::timed_out) {
    return "TCP probe timed out";
  }
  if (error == asio::error::host_not_found || error == asio::error::host_not_found_try_again) {
    return "Host not found";
  }
  if (error == asio::error::network_unreachable) {
    return "Network unreachable";
  }
  if (error == asio::error::connection_refused) {
    return "Connection refused";
  }
  if (error == asio::error::host_unreachable) {
    return "Host unreachable";
  }

  return std::format("{} ({})", error.message(), error.value());
}

auto make_timeout_result() -> TcpProbeResult {
  return TcpProbeResult{.reachable = false,
                        .error_code = static_cast<int>(asio::error::timed_out),
                        .reason = "TCP probe timed out"};
}

auto parse_port(const std::wstring& port) -> std::expected<unsigned short, std::string> {
  auto port_utf8 = Utils::String::ToUtf8(port);
  if (port_utf8.empty()) {
    return std::unexpected("Port is empty");
  }

  unsigned int port_value = 0;
  auto [ptr, ec] =
      std::from_chars(port_utf8.data(), port_utf8.data() + port_utf8.size(), port_value);
  if (ec != std::errc{} || ptr != port_utf8.data() + port_utf8.size() || port_value > 65535) {
    return std::unexpected("Invalid TCP port: " + port_utf8);
  }

  return static_cast<unsigned short>(port_value);
}

template <typename CancelFn, typename AwaitFn>
auto with_timeout(CancelFn&& cancel_fn, AwaitFn&& await_fn, std::chrono::milliseconds timeout)
    -> asio::awaitable<std::pair<bool, asio::error_code>> {
  auto executor = co_await asio::this_coro::executor;
  asio::steady_timer timer(executor);
  bool completed = false;
  bool timed_out = false;

  timer.expires_after(timeout);
  timer.async_wait([&](const asio::error_code& error) {
    if (error || completed) {
      return;
    }

    // timer 是探测的硬边界；超时后取消/关闭 socket，避免启动被网络黑洞拖住。
    timed_out = true;
    cancel_fn();
  });

  asio::error_code operation_error;
  co_await await_fn(operation_error);
  completed = true;

  asio::error_code ignored;
  timer.cancel(ignored);

  co_return std::pair{timed_out, operation_error};
}

auto connect_endpoint(asio::ip::tcp::endpoint endpoint, std::chrono::milliseconds timeout)
    -> asio::awaitable<TcpProbeResult> {
  auto executor = co_await asio::this_coro::executor;
  asio::ip::tcp::socket socket(executor);

  auto [timed_out, connect_error] = co_await with_timeout(
      [&]() {
        asio::error_code ignored;
        socket.cancel(ignored);
        socket.close(ignored);
      },
      [&](asio::error_code& operation_error) -> asio::awaitable<void> {
        co_await socket.async_connect(endpoint,
                                      asio::redirect_error(asio::use_awaitable, operation_error));
      },
      timeout);

  if (timed_out) {
    co_return make_timeout_result();
  }

  if (connect_error) {
    co_return TcpProbeResult{.reachable = false,
                             .error_code = connect_error.value(),
                             .reason = reason_from_error(connect_error)};
  }

  co_return TcpProbeResult{.reachable = true};
}

auto resolve_host(const std::string& server, const std::string& port,
                  std::chrono::milliseconds timeout)
    -> asio::awaitable<std::expected<asio::ip::tcp::resolver::results_type, TcpProbeResult>> {
  auto executor = co_await asio::this_coro::executor;
  asio::ip::tcp::resolver resolver(executor);

  asio::ip::tcp::resolver::results_type results;
  auto [timed_out, resolve_error] = co_await with_timeout(
      [&]() { resolver.cancel(); },
      [&](asio::error_code& operation_error) -> asio::awaitable<void> {
        results = co_await resolver.async_resolve(
            server, port, asio::redirect_error(asio::use_awaitable, operation_error));
      },
      timeout);

  if (timed_out) {
    co_return std::unexpected(make_timeout_result());
  }

  if (resolve_error) {
    co_return std::unexpected(TcpProbeResult{.reachable = false,
                                             .error_code = resolve_error.value(),
                                             .reason = reason_from_error(resolve_error)});
  }

  co_return results;
}

auto connect_resolved(asio::ip::tcp::resolver::results_type endpoints,
                      std::chrono::milliseconds timeout) -> asio::awaitable<TcpProbeResult> {
  auto executor = co_await asio::this_coro::executor;
  asio::ip::tcp::socket socket(executor);

  auto [timed_out, connect_error] = co_await with_timeout(
      [&]() {
        asio::error_code ignored;
        socket.cancel(ignored);
        socket.close(ignored);
      },
      [&](asio::error_code& operation_error) -> asio::awaitable<void> {
        co_await asio::async_connect(socket, endpoints,
                                     asio::redirect_error(asio::use_awaitable, operation_error));
      },
      timeout);

  if (timed_out) {
    co_return make_timeout_result();
  }

  if (connect_error) {
    co_return TcpProbeResult{.reachable = false,
                             .error_code = connect_error.value(),
                             .reason = reason_from_error(connect_error)};
  }

  co_return TcpProbeResult{.reachable = true};
}

}  // namespace Utils::Network::Detail

namespace Utils::Network {

auto probe_tcp_port(const std::wstring& server, const std::wstring& port,
                    std::chrono::milliseconds timeout) -> asio::awaitable<TcpProbeResult> {
  if (server.empty()) {
    co_return TcpProbeResult{.reachable = false, .reason = "Server name is empty"};
  }

  auto port_result = Detail::parse_port(port);
  if (!port_result) {
    co_return TcpProbeResult{.reachable = false, .reason = port_result.error()};
  }

  auto server_utf8 = Utils::String::ToUtf8(server);
  auto port_utf8 = Utils::String::ToUtf8(port);
  if (server_utf8.empty()) {
    co_return TcpProbeResult{.reachable = false, .reason = "Server name is empty"};
  }

  asio::error_code address_error;
  auto address = asio::ip::make_address(server_utf8, address_error);
  if (!address_error) {
    // IP literal 不走 resolver，避免名称解析把短超时探测放大。
    co_return co_await Detail::connect_endpoint(
        asio::ip::tcp::endpoint(address, port_result.value()), timeout);
  }

  auto resolve_result = co_await Detail::resolve_host(server_utf8, port_utf8, timeout);
  if (!resolve_result) {
    co_return resolve_result.error();
  }

  co_return co_await Detail::connect_resolved(resolve_result.value(), timeout);
}

}  // namespace Utils::Network
