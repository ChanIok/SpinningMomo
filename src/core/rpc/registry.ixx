module;

export module Core.RPC.Registry;

import std;
import Core.State;

namespace Core::RPC::Registry {

// 注册所有RPC端点
export auto register_all_endpoints(Core::State::AppState& state) -> void;

}  // namespace Core::RPC::Registry
