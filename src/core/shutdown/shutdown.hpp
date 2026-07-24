#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::shutdown {

auto shutdown_application(core::AppState& state) -> void;

}
