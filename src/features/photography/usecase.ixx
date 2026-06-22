module;

export module Features.Photography.UseCase;

import std;
import Core.State;

namespace Features::Photography::UseCase {

export auto start(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto stop(Core::State::AppState& state) -> void;
export auto toggle(Core::State::AppState& state) -> void;
export auto cleanup(Core::State::AppState& state) -> void;
export auto handle_panel_close(Core::State::AppState& state) -> void;

}  // namespace Features::Photography::UseCase
