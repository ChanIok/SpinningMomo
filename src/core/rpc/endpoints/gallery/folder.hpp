#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::rpc::endpoints::gallery::folder {

auto register_all(core::AppState& app_state) -> void;

}  // namespace core::rpc::endpoints::gallery::folder
