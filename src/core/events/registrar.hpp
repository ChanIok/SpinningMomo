#pragma once

#include "core/state/app_state.hpp"

namespace Core::Events {

auto register_all_handlers(Core::State::AppState& app_state) -> void;

}  // namespace Core::Events