module;

export module UI.PhotographyPanel;

import std;
import Core.State;

namespace UI::PhotographyPanel {

export auto show(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto hide(Core::State::AppState& state) -> void;
export auto request_repaint(Core::State::AppState& state) -> void;
export auto refresh_from_settings(Core::State::AppState& state) -> void;
export auto cleanup(Core::State::AppState& state) -> void;

}  // namespace UI::PhotographyPanel
