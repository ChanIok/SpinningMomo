module;

export module Plugins.InfinityNikki.ScreenshotShortcuts;

import std;
import Core.State;
import Plugins.InfinityNikki.Types;

namespace Plugins::InfinityNikki::ScreenshotShortcuts {

export auto initialize(
    Core::State::AppState& app_state,
    const std::function<void(const InfinityNikkiInitializeScreenshotShortcutsProgress&)>&
        progress_callback = nullptr)
    -> std::expected<InfinityNikkiInitializeScreenshotShortcutsResult, std::string>;

export auto sync(Core::State::AppState& app_state)
    -> std::expected<InfinityNikkiInitializeScreenshotShortcutsResult, std::string>;

// 返回 PhotoService 应监听的目录（GamePlayPhotos 目录），供 Gallery.Watcher 注册使用
export auto resolve_watch_directory(Core::State::AppState& app_state)
    -> std::expected<std::filesystem::path, std::string>;

}  // namespace Plugins::InfinityNikki::ScreenshotShortcuts
