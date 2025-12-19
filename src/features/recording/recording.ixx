module;

export module Features.Recording;

import std;
import Features.Recording.Types;
import Features.Recording.State;
import <windows.h>;

export namespace Features::Recording {

// 初始化录制模块
auto initialize(Features::Recording::State::RecordingState& state)
    -> std::expected<void, std::string>;

// 开始录制
auto start(Features::Recording::State::RecordingState& state, HWND target_window,
           const Features::Recording::Types::RecordingConfig& config)
    -> std::expected<void, std::string>;

// 停止录制
auto stop(Features::Recording::State::RecordingState& state) -> void;

// 清理资源
auto cleanup(Features::Recording::State::RecordingState& state) -> void;

}  // namespace Features::Recording
