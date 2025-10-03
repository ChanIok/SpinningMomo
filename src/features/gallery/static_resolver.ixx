module;

export module Features.Gallery.StaticResolver;

import std;
import Core.State;

namespace Features::Gallery::StaticResolver {

// 为 HTTP 静态服务注册解析器
export auto register_http_resolvers(Core::State::AppState& state) -> void;

// 为 WebView 注册解析器
export auto register_webview_resolvers(Core::State::AppState& state) -> void;

// 注销所有解析器（清理时调用）
export auto unregister_all_resolvers(Core::State::AppState& state) -> void;

}  // namespace Features::Gallery::StaticResolver
