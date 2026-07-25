#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::rpc::endpoints::file {

auto register_all(core::AppState& state) -> void;

}  // namespace core::rpc::endpoints::file
