#pragma once

#include <wil/com.h>
#include <windows.h>

#include <WebView2.h>

#include "core/state/app_state.hpp"
#include "core/webview/state.hpp"
#include "core/webview/types.hpp"

namespace Core::WebView::Static {

// 注册 WebView 资源解析器（接受 AppState）
auto register_web_resource_resolver(Core::State::AppState& state, std::wstring prefix,
                                    Types::WebResourceResolver resolver) -> void;

// 设置 WebResourceRequested 拦截
auto setup_resource_interception(Core::State::AppState& state, ICoreWebView2* webview,
                                 ICoreWebView2Environment* environment,
                                 Core::WebView::State::CoreResources& resources,
                                 Core::WebView::State::WebViewConfig& config) -> HRESULT;

}  // namespace Core::WebView::Static
