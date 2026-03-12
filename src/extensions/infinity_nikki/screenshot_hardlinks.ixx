module;

export module Extensions.InfinityNikki.ScreenshotHardlinks;

import std;
import Core.State;
import Extensions.InfinityNikki.Types;

namespace Extensions::InfinityNikki::ScreenshotHardlinks {

export auto initialize(
    Core::State::AppState& app_state,
    const std::function<void(const InfinityNikkiInitializeScreenshotHardlinksProgress&)>&
        progress_callback = nullptr)
    -> std::expected<InfinityNikkiInitializeScreenshotHardlinksResult, std::string>;

export auto sync(Core::State::AppState& app_state)
    -> std::expected<InfinityNikkiInitializeScreenshotHardlinksResult, std::string>;

// 返回 PhotoService 应监听的目录（GamePlayPhotos 目录），供 Gallery.Watcher 注册使用
export auto resolve_watch_directory(Core::State::AppState& app_state)
    -> std::expected<std::filesystem::path, std::string>;

}  // namespace Extensions::InfinityNikki::ScreenshotHardlinks
