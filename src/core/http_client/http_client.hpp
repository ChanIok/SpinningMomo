#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/http_client/types.hpp"
#include "core/state/app_state.hpp"

namespace core::http_client {

auto initialize(core::AppState& state) -> std::expected<void, std::string>;

auto shutdown(core::AppState& state) -> void;

auto fetch(core::AppState& state, const core::http_client::Request& request)
    -> asio::awaitable<std::expected<core::http_client::Response, std::string>>;

auto download_to_file(core::AppState& state, const core::http_client::Request& request,
                      const std::filesystem::path& output_path,
                      core::http_client::DownloadProgressCallback progress_callback = nullptr)
    -> asio::awaitable<std::expected<void, std::string>>;

}  // namespace core::http_client
