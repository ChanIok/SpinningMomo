module;

export module UI.TrayIcon;

import std;
import Core.State;

namespace UI::TrayIcon {

export auto create(Core::State::AppState& state) -> std::expected<void, std::string>;

export auto destroy(Core::State::AppState& state) -> void;

export auto show_context_menu(Core::State::AppState& state) -> void;

}  // namespace UI::TrayIcon