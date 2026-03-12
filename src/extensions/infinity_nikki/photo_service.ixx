module;

export module Extensions.InfinityNikki.PhotoService;

import std;
import Core.State;

namespace Extensions::InfinityNikki::PhotoService {

// 根据当前设置决定是否向 Gallery.Watcher 注册无限暖暖目录监听。
// 监听触发扫描后，回调驱动 ScreenShot 硬链接同步与照片元数据提取。
// 须在 Features::Gallery::initialize 之后调用。
export auto register_from_settings(Core::State::AppState& app_state) -> void;

export auto refresh_from_settings(Core::State::AppState& app_state) -> void;

export auto shutdown(Core::State::AppState& app_state) -> void;

}  // namespace Extensions::InfinityNikki::PhotoService
