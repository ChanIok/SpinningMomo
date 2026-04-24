module;

export module Extensions.InfinityNikki.MediaHardlinks;

import std;
import Core.State;
import Extensions.InfinityNikki.Types;
import Features.Gallery.Types;

namespace Extensions::InfinityNikki::MediaHardlinks {

export auto initialize(
    Core::State::AppState& app_state,
    const std::function<void(const InfinityNikkiInitializeMediaHardlinksProgress&)>&
        progress_callback = nullptr)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string>;

export auto sync(Core::State::AppState& app_state)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string>;

// 运行时增量同步入口：根据 Gallery 产出的变化集修正受影响的受管硬链接。
// 目前同时覆盖照片与录像两类媒体投影。
export auto apply_runtime_changes(Core::State::AppState& app_state,
                                  const std::vector<Features::Gallery::Types::ScanChange>& changes)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string>;

// 统一消费一次 Gallery 扫描结果：
// - 有逐文件 changes：走增量同步
// - 完全无变化：直接 no-op
// - 有变化但缺少 changes：回退到全量 sync
export auto apply_scan_result(Core::State::AppState& app_state,
                              const Features::Gallery::Types::ScanResult& result)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string>;

// 返回 PhotoService 应监听的目录（GamePlayPhotos 目录），供 Gallery.Watcher 注册使用
export auto resolve_watch_directory(Core::State::AppState& app_state)
    -> std::expected<std::filesystem::path, std::string>;

}  // namespace Extensions::InfinityNikki::MediaHardlinks
