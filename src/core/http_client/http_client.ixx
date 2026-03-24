module;

#include <asio.hpp>

export module Core.HttpClient;

import std;
import Core.State;
import Core.HttpClient.Types;

namespace Core::HttpClient {

export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;

export auto shutdown(Core::State::AppState& state) -> void;

export auto fetch(Core::State::AppState& state, const Core::HttpClient::Types::Request& request)
    -> asio::awaitable<std::expected<Core::HttpClient::Types::Response, std::string>>;

export auto download_to_file(
    Core::State::AppState& state, const Core::HttpClient::Types::Request& request,
    const std::filesystem::path& output_path,
    Core::HttpClient::Types::DownloadProgressCallback progress_callback = nullptr)
    -> asio::awaitable<std::expected<void, std::string>>;

}  // namespace Core::HttpClient
