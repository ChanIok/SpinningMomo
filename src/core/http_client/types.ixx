module;

export module Core.HttpClient.Types;

import std;

namespace Core::HttpClient::Types {

export struct DownloadProgress {
  std::uint64_t downloaded_bytes = 0;
  std::optional<std::uint64_t> total_bytes;
};

export using DownloadProgressCallback = std::function<void(const DownloadProgress&)>;

export struct Header {
  std::string name;
  std::string value;
};

export struct Request {
  std::string method = "GET";
  std::string url;
  std::vector<Header> headers;
  std::string body;
  std::optional<std::int32_t> connect_timeout_ms = std::nullopt;
  std::optional<std::int32_t> send_timeout_ms = std::nullopt;
  std::optional<std::int32_t> receive_timeout_ms = std::nullopt;
};

export struct Response {
  std::int32_t status_code = 0;
  std::string body;
  std::vector<Header> headers;
};

}  // namespace Core::HttpClient::Types
