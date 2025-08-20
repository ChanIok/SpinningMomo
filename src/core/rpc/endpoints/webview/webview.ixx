module;

export module Core.RPC.Endpoints.WebView;

import Core.State;

namespace Core::RPC::Endpoints::WebView {

// 注册RPC方法
export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::WebView