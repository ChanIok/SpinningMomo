module;

export module Core.RPC.NotificationHub;

import std;
import Core.State;

namespace Core::RPC::NotificationHub {

// 同时分发到 WebView 和 SSE 的统一通知出口
export auto send_notification(Core::State::AppState& state, const std::string& method,
                              const std::string& params_json = "{}") -> void;

}  // namespace Core::RPC::NotificationHub
