module;

export module Handlers.EventRegistrar;

import Core.State;

namespace Handlers {

export auto register_all_handlers(Core::State::AppState& app_state) -> void;

}