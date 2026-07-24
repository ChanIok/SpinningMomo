#include "core/rpc/notification_hub.hpp"

#include "vendor/std.hpp"

#include "core/events/events.hpp"
#include "core/http_server/sse_manager.hpp"
#include "core/state/app_state.hpp"
#include "core/webview/events.hpp"
#include "utils/logger/logger.hpp"

namespace core::rpc::notification_hub {

auto build_json_rpc_notification(const std::string& method, const std::string& params_json)
    -> std::string {
  return std::format(R"({{"jsonrpc":"2.0","method":"{}","params":{}}})", method, params_json);
}

auto send_notification(core::AppState& state, const std::string& method,
                       const std::string& params_json) -> void {
  auto payload = build_json_rpc_notification(method, params_json);

  if (state.events) {
    // watcher 可能在后台线程触发，这里先丢回主线程再发给 WebView。
    core::events::post(state, core::webview::events::WebViewResponseEvent{payload});
  }

  // 浏览器开发模式也用同一份通知（SSE）。
  core::http_server::sse_manager::broadcast_event(state, payload);

  Logger().debug("Notification dispatched: {}", method);
}

}  // namespace core::rpc::notification_hub
