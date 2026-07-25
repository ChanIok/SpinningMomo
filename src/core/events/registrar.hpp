#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::events {

auto register_all_handlers(core::AppState& app_state) -> void;

}  // namespace core::events
