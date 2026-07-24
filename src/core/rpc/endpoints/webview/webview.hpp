#pragma once

#include "core/state/app_state.hpp"

namespace Core::RPC::Endpoints::WebView {

// 注册RPC方法
auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::WebView