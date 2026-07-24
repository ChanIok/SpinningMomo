#pragma once

#include "core/state/app_state.hpp"

namespace Core::RuntimeInfo {

// 采集运行时信息并写入 state.runtime_info，同时输出关键日志
auto collect(Core::State::AppState& app_state) -> void;

}  // namespace Core::RuntimeInfo
