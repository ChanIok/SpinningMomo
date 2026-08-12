#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::rpc::endpoints::lan_access {

// 注册本机 LAN 设置页所需的状态查询和令牌轮换接口。
auto register_all(core::AppState& app_state) -> void;

}  // namespace core::rpc::endpoints::lan_access
