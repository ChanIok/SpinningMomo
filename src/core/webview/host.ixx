module;

export module Core.WebView.Host;

import std;
import Core.State;
import <windows.h>;

namespace Core::WebView::Host {

export auto start_environment_creation(Core::State::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string>;

export auto reset_host_runtime(Core::State::AppState& state) -> void;

export auto apply_background_mode_from_settings(Core::State::AppState& state) -> void;

export auto get_loading_background_color(Core::State::AppState& state) -> COLORREF;

}  // namespace Core::WebView::Host
