module;

#include <wil/com.h>
#include <WebView2.h>

export module Core.WebView.Static;

import std;
import Core.State;
import Core.WebView.Types;
import Core.WebView.State;

namespace Core::WebView::Static {

// 注册 WebView 资源解析器（接受 AppState）
export auto register_web_resource_resolver(Core::State::AppState& state, std::wstring prefix,
                                           Types::WebResourceResolver resolver) -> void;

// 注销 WebView 资源解析器
export auto unregister_web_resource_resolver(Core::State::AppState& state, std::wstring_view prefix)
    -> void;

// 设置 WebResourceRequested 拦截
export auto setup_resource_interception(Core::State::AppState& state, ICoreWebView2* webview,
                                        ICoreWebView2Environment* environment,
                                        Core::WebView::State::CoreResources& resources,
                                        Core::WebView::State::WebViewConfig& config) -> HRESULT;

}  // namespace Core::WebView::Static
