module;

#include <mfidl.h>
#include <mfreadwrite.h>

module Features.ReplayBuffer.Muxer;

import std;
import Features.ReplayBuffer.DiskRingBuffer;
import Utils.Logger;
import <d3d11.h>;
import <mfapi.h>;
import <mferror.h>;
import <wil/com.h>;

namespace Features::ReplayBuffer::Muxer {

auto mux_frames_to_mp4(
    const std::vector<Features::ReplayBuffer::DiskRingBuffer::FrameMetadata>& frames,
    Features::ReplayBuffer::DiskRingBuffer::DiskRingBufferContext& ring_buffer,
    IMFMediaType* video_type, IMFMediaType* audio_type, const std::filesystem::path& output_path)
    -> std::expected<void, std::string> {
  if (frames.empty()) {
    return std::unexpected("No frames to mux");
  }

  if (!video_type) {
    return std::unexpected("Video type is required");
  }

  // 确保目录存在
  std::filesystem::create_directories(output_path.parent_path());

  // 1. 创建 SinkWriter 属性
  wil::com_ptr<IMFAttributes> attributes;
  HRESULT hr = MFCreateAttributes(attributes.put(), 2);
  if (FAILED(hr)) {
    return std::unexpected("Failed to create MF attributes");
  }

  // 禁用编码器（stream copy 模式）
  attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);

  // 2. 创建 SinkWriter
  wil::com_ptr<IMFSinkWriter> sink_writer;
  hr = MFCreateSinkWriterFromURL(output_path.c_str(), nullptr, attributes.get(), sink_writer.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create sink writer: " + std::to_string(hr));
  }

  // 3. 添加视频流（直接使用压缩后的类型）
  DWORD video_stream_index = 0;
  hr = sink_writer->AddStream(video_type, &video_stream_index);
  if (FAILED(hr)) {
    return std::unexpected("Failed to add video stream: " + std::to_string(hr));
  }

  // 设置输入类型（与输出相同，实现 stream copy）
  hr = sink_writer->SetInputMediaType(video_stream_index, video_type, nullptr);
  if (FAILED(hr)) {
    return std::unexpected("Failed to set video input type: " + std::to_string(hr));
  }

  // 4. 添加音频流（可选）
  DWORD audio_stream_index = 0;
  bool has_audio = (audio_type != nullptr);
  if (has_audio) {
    hr = sink_writer->AddStream(audio_type, &audio_stream_index);
    if (FAILED(hr)) {
      Logger().warn("Failed to add audio stream, continuing without audio");
      has_audio = false;
    } else {
      hr = sink_writer->SetInputMediaType(audio_stream_index, audio_type, nullptr);
      if (FAILED(hr)) {
        Logger().warn("Failed to set audio input type, continuing without audio");
        has_audio = false;
      }
    }
  }

  // 5. 开始写入
  hr = sink_writer->BeginWriting();
  if (FAILED(hr)) {
    return std::unexpected("Failed to begin writing: " + std::to_string(hr));
  }

  // 6. 计算时间戳偏移（使第一帧从 0 开始）
  std::int64_t first_video_timestamp = 0;
  std::int64_t first_audio_timestamp = 0;

  for (const auto& frame : frames) {
    if (!frame.is_audio) {
      first_video_timestamp = frame.timestamp_100ns;
      break;
    }
  }

  for (const auto& frame : frames) {
    if (frame.is_audio) {
      first_audio_timestamp = frame.timestamp_100ns;
      break;
    }
  }

  // 7. 写入所有帧
  std::uint32_t video_count = 0;
  std::uint32_t audio_count = 0;

  for (const auto& frame : frames) {
    // 从磁盘读取帧数据
    auto frame_data = DiskRingBuffer::read_frame(ring_buffer, frame);
    if (!frame_data) {
      Logger().warn("Failed to read frame data: {}", frame_data.error());
      continue;
    }

    // 创建 sample
    wil::com_ptr<IMFSample> sample;
    hr = MFCreateSample(sample.put());
    if (FAILED(hr)) {
      Logger().warn("Failed to create sample");
      continue;
    }

    // 创建 buffer
    wil::com_ptr<IMFMediaBuffer> buffer;
    hr = MFCreateMemoryBuffer(static_cast<DWORD>(frame_data->size()), buffer.put());
    if (FAILED(hr)) {
      Logger().warn("Failed to create buffer");
      continue;
    }

    // 复制数据到 buffer
    BYTE* dest = nullptr;
    hr = buffer->Lock(&dest, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      std::memcpy(dest, frame_data->data(), frame_data->size());
      buffer->Unlock();
      buffer->SetCurrentLength(static_cast<DWORD>(frame_data->size()));
    } else {
      Logger().warn("Failed to lock buffer");
      continue;
    }

    sample->AddBuffer(buffer.get());

    // 设置时间戳（相对于第一帧）
    std::int64_t base_timestamp = frame.is_audio ? first_audio_timestamp : first_video_timestamp;
    std::int64_t adjusted_timestamp = frame.timestamp_100ns - base_timestamp;
    if (adjusted_timestamp < 0) {
      adjusted_timestamp = 0;
    }

    sample->SetSampleTime(adjusted_timestamp);

    if (frame.duration_100ns > 0) {
      sample->SetSampleDuration(frame.duration_100ns);
    }

    // 标记关键帧
    if (frame.is_keyframe) {
      sample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
    }

    // 写入 sample
    DWORD stream_index = frame.is_audio ? audio_stream_index : video_stream_index;
    if (frame.is_audio && !has_audio) {
      continue;  // 跳过音频帧
    }

    hr = sink_writer->WriteSample(stream_index, sample.get());
    if (FAILED(hr)) {
      Logger().warn("Failed to write sample: {}", hr);
      continue;
    }

    if (frame.is_audio) {
      audio_count++;
    } else {
      video_count++;
    }
  }

  // 8. 完成写入
  hr = sink_writer->Finalize();
  if (FAILED(hr)) {
    return std::unexpected("Failed to finalize: " + std::to_string(hr));
  }

  Logger().info("Muxed {} video frames, {} audio frames to {}", video_count, audio_count,
                output_path.string());

  return {};
}

}  // namespace Features::ReplayBuffer::Muxer
