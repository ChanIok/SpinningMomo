#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::rpc::registry {

// 注册所有RPC端点
auto register_all_endpoints(core::AppState& state) -> void;

}  // namespace core::rpc::registry
