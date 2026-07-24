#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"

namespace Core::WebView::Host {

auto start_environment_creation(Core::State::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string>;

auto reset_host_runtime(Core::State::AppState& state) -> void;

auto apply_background_mode_from_settings(Core::State::AppState& state) -> void;

auto get_loading_background_color(Core::State::AppState& state) -> COLORREF;

}  // namespace Core::WebView::Host
