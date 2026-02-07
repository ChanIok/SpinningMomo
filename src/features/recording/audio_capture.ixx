module;

export module Features.Recording.AudioCapture;

import std;
import Features.Recording.State;
import Features.Recording.Types;
import Utils.Media.AudioCapture;

export namespace Features::Recording::AudioCapture {

// 初始化音频捕获（委托给 Utils.Media.AudioCapture）
auto initialize(Utils::Media::AudioCapture::AudioCaptureContext& ctx,
                Features::Recording::Types::AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string>;

// 启动音频捕获线程（为 Recording 创建回调）
auto start_capture_thread(Features::Recording::State::RecordingState& state) -> void;

// 停止音频捕获
auto stop(Utils::Media::AudioCapture::AudioCaptureContext& ctx) -> void;

// 清理音频资源
auto cleanup(Utils::Media::AudioCapture::AudioCaptureContext& ctx) -> void;

}  // namespace Features::Recording::AudioCapture
