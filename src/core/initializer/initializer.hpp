#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::initializer {

auto initialize_application(core::AppState& state) -> std::expected<void, std::string>;

}
