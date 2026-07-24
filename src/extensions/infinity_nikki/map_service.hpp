#pragma once

#include "core/state/app_state.hpp"

namespace Extensions::InfinityNikki::MapService {

// 注册 Infinity Nikki 官方地图页面所需的 WebView 注入脚本。
auto register_from_settings(Core::State::AppState& app_state) -> void;

}  // namespace Extensions::InfinityNikki::MapService
