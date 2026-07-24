#pragma once

#include "core/state/app_state.hpp"

namespace Core::RPC::Registry {

// 注册所有RPC端点
auto register_all_endpoints(Core::State::AppState& state) -> void;

}  // namespace Core::RPC::Registry
