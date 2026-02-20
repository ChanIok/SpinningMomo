module;

module Core.RPC.NotificationHub;

import std;
import Core.State;
import Core.HttpServer.SseManager;
import Core.WebView.RpcBridge;
import Utils.Logger;

namespace Core::RPC::NotificationHub {

auto build_json_rpc_notification(const std::string& method, const std::string& params_json)
    -> std::string {
  return std::format(R"({{"jsonrpc":"2.0","method":"{}","params":{}}})", method, params_json);
}

auto send_notification(Core::State::AppState& state, const std::string& method,
                       const std::string& params_json) -> void {
  Core::WebView::RpcBridge::send_notification(state, method, params_json);

  auto payload = build_json_rpc_notification(method, params_json);
  Core::HttpServer::SseManager::broadcast_event(state, payload);

  Logger().debug("Notification dispatched: {}", method);
}

}  // namespace Core::RPC::NotificationHub
