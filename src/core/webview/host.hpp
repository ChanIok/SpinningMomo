#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace core::webview::host {

auto start_environment_creation(core::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string>;

auto reset_host_runtime(core::AppState& state) -> void;

auto apply_background_mode_from_settings(core::AppState& state) -> void;

auto get_loading_background_color(core::AppState& state) -> COLORREF;

}  // namespace core::webview::host
