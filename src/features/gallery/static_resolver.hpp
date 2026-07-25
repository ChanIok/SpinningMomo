#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::gallery::static_resolver {

// 为 HTTP 静态服务注册解析器
auto register_http_resolvers(core::AppState& state) -> void;

// 为 WebView 注册解析器
auto register_webview_resolvers(core::AppState& state) -> void;

// 注销所有解析器（清理时调用）
auto unregister_all_resolvers(core::AppState& state) -> void;

}  // namespace features::gallery::static_resolver
