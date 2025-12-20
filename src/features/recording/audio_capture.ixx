module;

export module Features.Recording.AudioCapture;

import std;
import Features.Recording.State;
import Features.Recording.Types;

export namespace Features::Recording::AudioCapture {

// 初始化音频捕获（根据音频源类型选择不同的初始化方式）
auto initialize(Features::Recording::State::AudioCaptureContext& ctx,
                Features::Recording::Types::AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string>;

// 启动音频捕获线程
auto start_capture_thread(Features::Recording::State::RecordingState& state) -> void;

// 停止音频捕获
auto stop(Features::Recording::State::AudioCaptureContext& ctx) -> void;

// 清理音频资源
auto cleanup(Features::Recording::State::AudioCaptureContext& ctx) -> void;

}  // namespace Features::Recording::AudioCapture
