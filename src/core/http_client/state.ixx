module;

export module Core.HttpClient.State;

import std;
import Core.HttpClient.Types;
import Vendor.WinHttp;
import <asio.hpp>;

export namespace Core::HttpClient::State {

struct RequestOperation {
  std::shared_ptr<RequestOperation> keepalive;
  std::mutex keepalive_mutex;

  Core::HttpClient::Types::Request request;
  Core::HttpClient::Types::Response response;
  std::expected<Core::HttpClient::Types::Response, std::string> result =
      std::unexpected("Request is not completed");

  asio::any_io_executor executor;
  std::optional<asio::steady_timer> completion_timer = std::nullopt;

  std::wstring wide_url;
  std::wstring wide_method;
  std::wstring wide_host;
  std::wstring wide_path;
  std::wstring wide_headers;

  std::vector<char> request_body;
  std::array<char, 16 * 1024> read_buffer{};

  Vendor::WinHttp::HINTERNET connect_handle = nullptr;
  Vendor::WinHttp::HINTERNET request_handle = nullptr;

  Vendor::WinHttp::INTERNET_PORT port = 0;
  bool secure = false;
  bool callback_registered = false;
  bool receive_started = false;

  std::atomic<bool> completed{false};
  std::atomic<bool> waiter_notified{false};
  std::atomic<bool> close_requested{false};
};

struct HttpClientState {
  Vendor::WinHttp::UniqueHInternet session;
  std::wstring user_agent = L"SpinningMomo/1.0";

  int resolve_timeout_ms = 0;
  int connect_timeout_ms = 10'000;
  int send_timeout_ms = 30'000;
  int receive_timeout_ms = 30'000;

  std::atomic<bool> is_initialized{false};
};

}  // namespace Core::HttpClient::State
