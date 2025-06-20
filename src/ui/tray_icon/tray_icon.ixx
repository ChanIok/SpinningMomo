module;

export module UI.TrayIcon;

import std;
import Core.State;
import Vendor.Windows;

namespace UI::TrayIcon {

export auto create(Core::State::AppState& state) -> std::expected<void, std::string>;

export auto destroy(Core::State::AppState& state) -> void;

export auto show_context_menu(Core::State::AppState& state) -> void;

export auto show_quick_menu(Core::State::AppState& state, const Vendor::Windows::POINT& pt) -> void;

}  // namespace UI::TrayIcon