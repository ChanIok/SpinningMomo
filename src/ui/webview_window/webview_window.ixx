module;

export module UI.WebViewWindow;

import std;
import Core.State;
import Vendor.Windows;

namespace UI::WebViewWindow {

// 窗口初始化和清理
export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto cleanup(Core::State::AppState& state) -> void;

// 窗口显示控制
export auto toggle_visibility(Core::State::AppState& state) -> void;

}  // namespace UI::WebViewWindow