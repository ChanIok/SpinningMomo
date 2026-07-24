#pragma once

#include "vendor/std.hpp"

namespace core::webview::events {

// WebView响应事件
struct WebViewResponseEvent {
  std::string response;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

}  // namespace core::webview::events
