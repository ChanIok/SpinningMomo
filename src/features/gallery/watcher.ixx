module;

export module Features.Gallery.Watcher;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Watcher {

// 启动时把已添加的根目录重新挂上监听，并先全量扫一遍。
export auto initialize_watchers(Core::State::AppState& app_state)
    -> std::expected<void, std::string>;

// 确保这个目录有 watcher（重复调用也安全）。
export auto ensure_watcher_for_directory(
    Core::State::AppState& app_state, const std::filesystem::path& root_directory,
    const std::optional<Types::ScanOptions>& scan_options = std::nullopt,
    bool bootstrap_full_scan = true) -> std::expected<void, std::string>;

// 退出时停掉所有 watcher 线程。
export auto shutdown_watchers(Core::State::AppState& app_state) -> void;

}  // namespace Features::Gallery::Watcher
