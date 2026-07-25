#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::rpc::endpoints::webview {

// 注册RPC方法
auto register_all(core::AppState& app_state) -> void;

}  // namespace core::rpc::endpoints::webview
