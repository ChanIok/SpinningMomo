module;

export module Features.Recording.UseCase;

import std;
import Core.State;

export namespace Features::Recording::UseCase {

// 切换录制状态 (开始/停止)
auto toggle_recording(Core::State::AppState& state) -> std::expected<void, std::string>;

// 停止录制 (如果正在运行)
auto stop_recording_if_running(Core::State::AppState& state) -> void;

}  // namespace Features::Recording::UseCase
