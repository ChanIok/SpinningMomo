#pragma once

#include "vendor/std.hpp"

#include "vendor/webview2.hpp"
#include "vendor/wil.hpp"
#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "core/webview/state.hpp"
#include "core/webview/types.hpp"

namespace core::webview::static_content {

// 注册 WebView 资源解析器（接受 AppState）
auto register_web_resource_resolver(core::AppState& state, std::wstring prefix,
                                    WebResourceResolver resolver) -> void;

// 设置 WebResourceRequested 拦截
auto setup_resource_interception(core::AppState& state, ICoreWebView2* webview,
                                 ICoreWebView2Environment* environment,
                                 core::webview::CoreResources& resources,
                                 core::webview::WebViewConfig& config) -> HRESULT;

}  // namespace core::webview::static_content
