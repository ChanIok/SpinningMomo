#pragma once

#include "core/state/app_state.hpp"

namespace Features::Gallery::StaticResolver {

// 为 HTTP 静态服务注册解析器
auto register_http_resolvers(Core::State::AppState& state) -> void;

// 为 WebView 注册解析器
auto register_webview_resolvers(Core::State::AppState& state) -> void;

// 注销所有解析器（清理时调用）
auto unregister_all_resolvers(Core::State::AppState& state) -> void;

}  // namespace Features::Gallery::StaticResolver
