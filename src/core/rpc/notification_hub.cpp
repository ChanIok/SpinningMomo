module;

module Core.RPC.NotificationHub;

import std;
import Core.State;
import Core.Events;
import Core.WebView.Events;
import Core.HttpServer.SseManager;
import Utils.Logger;

namespace Core::RPC::NotificationHub {

auto build_json_rpc_notification(const std::string& method, const std::string& params_json)
    -> std::string {
  return std::format(R"({{"jsonrpc":"2.0","method":"{}","params":{}}})", method, params_json);
}

auto send_notification(Core::State::AppState& state, const std::string& method,
                       const std::string& params_json) -> void {
  auto payload = build_json_rpc_notification(method, params_json);

  if (state.events) {
    // watcher 可能在后台线程触发，这里先丢回主线程再发给 WebView。
    Core::Events::post(*state.events, Core::WebView::Events::WebViewResponseEvent{payload});
  }

  // 浏览器开发模式也用同一份通知（SSE）。
  Core::HttpServer::SseManager::broadcast_event(state, payload);

  Logger().debug("Notification dispatched: {}", method);
}

}  // namespace Core::RPC::NotificationHub
