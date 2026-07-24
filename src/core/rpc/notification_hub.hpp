#pragma once

#include "core/state/app_state.hpp"

namespace Core::RPC::NotificationHub {

// 同时分发到 WebView 和 SSE 的统一通知出口
auto send_notification(Core::State::AppState& state, const std::string& method,
                       const std::string& params_json = "{}") -> void;

}  // namespace Core::RPC::NotificationHub
