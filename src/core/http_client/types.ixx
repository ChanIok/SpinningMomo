module;

export module Core.HttpClient.Types;

import std;

export namespace Core::HttpClient::Types {

struct Header {
  std::string name;
  std::string value;
};

struct Request {
  std::string method = "GET";
  std::string url;
  std::vector<Header> headers;
  std::string body;
  std::optional<std::int32_t> connect_timeout_ms = std::nullopt;
  std::optional<std::int32_t> send_timeout_ms = std::nullopt;
  std::optional<std::int32_t> receive_timeout_ms = std::nullopt;
};

struct Response {
  std::int32_t status_code = 0;
  std::string body;
  std::vector<Header> headers;
};

}  // namespace Core::HttpClient::Types
