module;

export module Core.WebView.Events;

import std;

namespace Core::WebView::Events {

// WebView响应事件
export struct WebViewResponseEvent {
  std::string response;
  
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

} // namespace Core::WebView::Events