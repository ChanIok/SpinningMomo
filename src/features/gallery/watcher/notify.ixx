module;

export module Features.Gallery.Watcher.Notify;

import std;
import Core.State;

namespace Features::Gallery::Watcher::Notify {

// 按 root key 启动目录监听主循环，线程入口从 AppState 定位一次状态。
export auto run_watch_loop(Core::State::AppState& app_state, const std::string& watcher_key,
                           std::stop_token stop_token) -> void;

}  // namespace Features::Gallery::Watcher::Notify
