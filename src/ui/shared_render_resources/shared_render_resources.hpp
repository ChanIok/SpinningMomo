#pragma once

#include "core/state/app_state.hpp"

namespace UI::SharedRenderResources {

auto ensure_initialized(Core::State::AppState& state) -> bool;
auto cleanup(Core::State::AppState& state) -> void;

}  // namespace UI::SharedRenderResources
