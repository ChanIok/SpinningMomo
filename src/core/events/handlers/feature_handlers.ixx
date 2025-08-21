module;

export module Core.Events.Handlers.Feature;

import Core.State;

namespace Core::Events::Handlers {

export auto register_feature_handlers(Core::State::AppState& app_state) -> void;

}