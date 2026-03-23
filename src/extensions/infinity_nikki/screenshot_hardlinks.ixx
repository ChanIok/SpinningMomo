module;

export module Extensions.InfinityNikki.ScreenshotHardlinks;

import std;
import Core.State;
import Extensions.InfinityNikki.Types;
import Features.Gallery.Types;

namespace Extensions::InfinityNikki::ScreenshotHardlinks {

export auto initialize(
    Core::State::AppState& app_state,
    const std::function<void(const InfinityNikkiInitializeScreenshotHardlinksProgress&)>&
        progress_callback = nullptr)
    -> std::expected<InfinityNikkiInitializeScreenshotHardlinksResult, std::string>;

export auto sync(Core::State::AppState& app_state)
    -> std::expected<InfinityNikkiInitializeScreenshotHardlinksResult, std::string>;

// 运行时增量同步入口：只根据 Gallery watcher 产出的变化集修正受影响的硬链接。
// 与 initialize()/sync() 的全量重建不同，这里不再重新扫描整个 GamePlayPhotos。
export auto apply_runtime_changes(Core::State::AppState& app_state,
                                  const std::vector<Features::Gallery::Types::ScanChange>& changes)
    -> std::expected<InfinityNikkiInitializeScreenshotHardlinksResult, std::string>;

// 返回 PhotoService 应监听的目录（GamePlayPhotos 目录），供 Gallery.Watcher 注册使用
export auto resolve_watch_directory(Core::State::AppState& app_state)
    -> std::expected<std::filesystem::path, std::string>;

}  // namespace Extensions::InfinityNikki::ScreenshotHardlinks
