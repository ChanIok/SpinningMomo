module;

export module Core.Initializer;

import std;
import Core.State;

namespace Core::Initializer {

export auto initialize_application(Core::State::AppState& state)
    -> std::expected<void, std::string>;

}