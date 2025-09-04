module;

export module Core.Shutdown;

import Core.State;

namespace Core::Shutdown {

export auto shutdown_application(Core::State::AppState& state) -> void;

}
