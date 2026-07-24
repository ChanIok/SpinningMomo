#pragma once

#include "core/state/app_state.hpp"

namespace Core::Shutdown {

auto shutdown_application(Core::State::AppState& state) -> void;

}
