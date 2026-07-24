#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::gallery::watcher::notify {

// 按 root key 启动目录监听主循环，线程入口从 AppState 定位一次状态。
auto run_watch_loop(core::AppState& app_state, const std::string& watcher_key,
                    std::stop_token stop_token) -> void;

}  // namespace features::gallery::watcher::notify
