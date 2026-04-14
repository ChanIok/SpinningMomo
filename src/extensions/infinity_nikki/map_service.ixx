module;

export module Extensions.InfinityNikki.MapService;

import Core.State;

namespace Extensions::InfinityNikki::MapService {

// 注册 Infinity Nikki 官方地图页面所需的 WebView 注入脚本。
export auto register_from_settings(Core::State::AppState& app_state) -> void;

}  // namespace Extensions::InfinityNikki::MapService
