#pragma once

#include <asio.hpp>

#include "core/http_client/types.hpp"
#include "core/state/app_state.hpp"

namespace Core::HttpClient {

auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;

auto shutdown(Core::State::AppState& state) -> void;

auto fetch(Core::State::AppState& state, const Core::HttpClient::Types::Request& request)
    -> asio::awaitable<std::expected<Core::HttpClient::Types::Response, std::string>>;

auto download_to_file(Core::State::AppState& state, const Core::HttpClient::Types::Request& request,
                      const std::filesystem::path& output_path,
                      Core::HttpClient::Types::DownloadProgressCallback progress_callback = nullptr)
    -> asio::awaitable<std::expected<void, std::string>>;

}  // namespace Core::HttpClient
