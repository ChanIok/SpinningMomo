module;

export module Features.Recording.UseCase;

import std;
import Core.State;

namespace Features::Recording::UseCase {

// 响应用户录制开关请求，负责查找窗口、组装配置并提交控制动作。
export auto toggle_recording(Core::State::AppState& state) -> std::expected<void, std::string>;

// 应用关闭时停止正在运行的录制，并等待控制线程完成收尾。
export auto stop_recording_if_running(Core::State::AppState& state) -> void;

}  // namespace Features::Recording::UseCase
