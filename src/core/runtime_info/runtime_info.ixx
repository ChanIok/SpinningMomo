module;

export module Core.RuntimeInfo;

import Core.State;

namespace Core::RuntimeInfo {

// 采集运行时信息并写入 state.runtime_info，同时输出关键日志
export auto collect(Core::State::AppState& app_state) -> void;

}  // namespace Core::RuntimeInfo
