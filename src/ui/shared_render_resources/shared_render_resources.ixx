module;

export module UI.SharedRenderResources;

import Core.State;

namespace UI::SharedRenderResources {

export auto ensure_initialized(Core::State::AppState& state) -> bool;
export auto cleanup(Core::State::AppState& state) -> void;

}  // namespace UI::SharedRenderResources
