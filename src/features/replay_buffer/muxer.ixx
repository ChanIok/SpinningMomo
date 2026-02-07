module;

#include <mfidl.h>

export module Features.ReplayBuffer.Muxer;

import std;
import Features.ReplayBuffer.DiskRingBuffer;
import <mfapi.h>;
import <wil/com.h>;

export namespace Features::ReplayBuffer::Muxer {

// 将压缩帧 mux 成 MP4（stream copy，不转码）
// frames: 要写入的帧元数据列表
// ring_buffer: 用于读取帧数据
// video_type: 视频输出媒体类型
// audio_type: 音频输出媒体类型（可选）
// output_path: 输出文件路径
auto mux_frames_to_mp4(
    const std::vector<Features::ReplayBuffer::DiskRingBuffer::FrameMetadata>& frames,
    Features::ReplayBuffer::DiskRingBuffer::DiskRingBufferContext& ring_buffer,
    IMFMediaType* video_type, IMFMediaType* audio_type, const std::filesystem::path& output_path)
    -> std::expected<void, std::string>;

}  // namespace Features::ReplayBuffer::Muxer
