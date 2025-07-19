module;

module Handlers.WebView;

import std;
import Core.Events;
import Core.State;
import Core.WebView;
import Utils.Logger;

namespace Handlers {

auto register_webview_handlers(Core::State::AppState& app_state) -> void {
  using namespace Core::Events;

  // 注册WebView响应事件处理器
  subscribe(*app_state.event_bus, EventType::WebViewResponse, [&app_state](const Event& event) {
    try {
      auto response_json = std::any_cast<std::string>(event.data);
      
      Logger().debug("Processing WebView response on UI thread");
      
      // 现在我们在UI线程上，可以安全调用WebView API
      Core::WebView::post_message(app_state, response_json);
      
      Logger().debug("WebView response sent successfully from UI thread");
      
    } catch (const std::exception& e) {
      Logger().error("Error processing WebView response event: {}", e.what());
    }
  });

  Logger().info("WebView handlers registered successfully");
}

}  // namespace Handlers 