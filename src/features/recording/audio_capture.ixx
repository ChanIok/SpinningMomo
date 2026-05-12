module;

export module Features.Recording.AudioCapture;

import std;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Media.AudioCapture;

namespace Features::Recording::AudioCapture {

// 初始化音频捕获上下文，内部委托给 Utils.Media.AudioCapture。
export auto initialize(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                       Utils::Media::AudioCapture::AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string>;

// 启动音频捕获线程，为当前录制段生产音频队列数据。
export auto start_capture_thread(Features::Recording::State::RecordingState& state) -> void;

// 停止音频捕获线程。
export auto stop(Utils::Media::AudioCapture::AudioCaptureContext& ctx) -> void;

// 清理音频捕获上下文资源。
export auto cleanup(Utils::Media::AudioCapture::AudioCaptureContext& ctx) -> void;

}  // namespace Features::Recording::AudioCapture
