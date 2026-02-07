module;

#include <mfidl.h>
#include <mfreadwrite.h>

module Features.ReplayBuffer.Trimmer;

import std;
import Utils.Logger;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::ReplayBuffer::Trimmer {

// 计算缩放后的分辨率
auto calculate_scaled_dimensions(std::uint32_t src_width, std::uint32_t src_height,
                                 std::uint32_t target_short_edge)
    -> std::pair<std::uint32_t, std::uint32_t> {
  // 确定短边是宽还是高
  bool width_is_shorter = src_width <= src_height;
  std::uint32_t short_edge = width_is_shorter ? src_width : src_height;
  std::uint32_t long_edge = width_is_shorter ? src_height : src_width;

  // 如果源短边已经小于等于目标，不缩放
  if (short_edge <= target_short_edge) {
    return {src_width, src_height};
  }

  // 计算缩放比例
  double scale = static_cast<double>(target_short_edge) / short_edge;
  std::uint32_t new_short = target_short_edge;
  std::uint32_t new_long = static_cast<std::uint32_t>(long_edge * scale);

  // 确保偶数（视频编码要求）
  new_short = (new_short / 2) * 2;
  new_long = (new_long / 2) * 2;

  if (width_is_shorter) {
    return {new_short, new_long};
  } else {
    return {new_long, new_short};
  }
}

auto scale_video(const std::filesystem::path& input_path, const std::filesystem::path& output_path,
                 const ScaleConfig& config) -> std::expected<void, std::string> {
  // 1. 探测源视频格式
  wil::com_ptr<IMFSourceReader> probe_reader;
  HRESULT hr = MFCreateSourceReaderFromURL(input_path.c_str(), nullptr, probe_reader.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to open input file: {:08X}", static_cast<uint32_t>(hr)));
  }

  wil::com_ptr<IMFMediaType> video_type;
  hr = probe_reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, video_type.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to get video media type");
  }

  // 获取源视频尺寸
  UINT32 src_width = 0, src_height = 0;
  MFGetAttributeSize(video_type.get(), MF_MT_FRAME_SIZE, &src_width, &src_height);

  // 计算目标分辨率
  auto [target_width, target_height] =
      calculate_scaled_dimensions(src_width, src_height, config.target_short_edge);

  Logger().info("Scaling video from {}x{} to {}x{}", src_width, src_height, target_width,
                target_height);

  // 检查是否有音频
  bool has_audio = false;
  hr = probe_reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr);
  if (SUCCEEDED(hr)) {
    has_audio = true;
  }

  probe_reader = nullptr;  // 释放

  // 2. 创建 SourceReader（解码 + 缩放）
  wil::com_ptr<IMFAttributes> reader_attrs;
  MFCreateAttributes(reader_attrs.put(), 2);
  reader_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
  reader_attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);

  wil::com_ptr<IMFSourceReader> reader;
  hr = MFCreateSourceReaderFromURL(input_path.c_str(), reader_attrs.get(), reader.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create source reader: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 设置视频解码输出为 NV12（目标分辨率）
  wil::com_ptr<IMFMediaType> video_decode_type;
  MFCreateMediaType(video_decode_type.put());
  video_decode_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  video_decode_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  MFSetAttributeSize(video_decode_type.get(), MF_MT_FRAME_SIZE, target_width, target_height);

  hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr,
                                   video_decode_type.get());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to set video decode type: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 音频解码为 PCM
  if (has_audio) {
    wil::com_ptr<IMFMediaType> audio_decode_type;
    MFCreateMediaType(audio_decode_type.put());
    audio_decode_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    audio_decode_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr,
                                     audio_decode_type.get());
    if (FAILED(hr)) {
      Logger().warn("Failed to set audio decode type, continuing without audio");
      has_audio = false;
    }
  }

  // 3. 创建 SinkWriter
  wil::com_ptr<IMFAttributes> sink_attrs;
  MFCreateAttributes(sink_attrs.put(), 1);
  sink_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

  wil::com_ptr<IMFSinkWriter> sink_writer;
  hr = MFCreateSinkWriterFromURL(output_path.c_str(), nullptr, sink_attrs.get(), sink_writer.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create sink writer: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 配置视频输出流（H.264）
  wil::com_ptr<IMFMediaType> output_video_type;
  MFCreateMediaType(output_video_type.put());
  output_video_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  output_video_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  output_video_type->SetUINT32(MF_MT_AVG_BITRATE, config.bitrate);
  MFSetAttributeSize(output_video_type.get(), MF_MT_FRAME_SIZE, target_width, target_height);
  MFSetAttributeRatio(output_video_type.get(), MF_MT_FRAME_RATE, config.fps, 1);
  MFSetAttributeRatio(output_video_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  output_video_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

  DWORD video_out_index = 0;
  hr = sink_writer->AddStream(output_video_type.get(), &video_out_index);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to add video stream: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 配置视频输入类型（NV12）
  wil::com_ptr<IMFMediaType> input_video_type;
  MFCreateMediaType(input_video_type.put());
  input_video_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  input_video_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  MFSetAttributeSize(input_video_type.get(), MF_MT_FRAME_SIZE, target_width, target_height);
  MFSetAttributeRatio(input_video_type.get(), MF_MT_FRAME_RATE, config.fps, 1);
  MFSetAttributeRatio(input_video_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  input_video_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

  hr = sink_writer->SetInputMediaType(video_out_index, input_video_type.get(), nullptr);
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to set video input type: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 配置音频流（AAC）
  DWORD audio_out_index = 0;
  if (has_audio) {
    wil::com_ptr<IMFMediaType> output_audio_type;
    MFCreateMediaType(output_audio_type.put());
    output_audio_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    output_audio_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    output_audio_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    output_audio_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    output_audio_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    output_audio_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);  // ~128kbps

    hr = sink_writer->AddStream(output_audio_type.get(), &audio_out_index);
    if (FAILED(hr)) {
      Logger().warn("Failed to add audio stream, continuing without audio");
      has_audio = false;
    } else {
      wil::com_ptr<IMFMediaType> input_audio_type;
      MFCreateMediaType(input_audio_type.put());
      input_audio_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
      input_audio_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
      input_audio_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
      input_audio_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
      input_audio_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);

      hr = sink_writer->SetInputMediaType(audio_out_index, input_audio_type.get(), nullptr);
      if (FAILED(hr)) {
        Logger().warn("Failed to set audio input type, continuing without audio");
        has_audio = false;
      }
    }
  }

  // 4. 开始写入
  hr = sink_writer->BeginWriting();
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to begin writing: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 5. 读取并写入 sample
  bool first_video_sample = true;
  std::int64_t first_video_timestamp = 0;
  bool first_audio_sample = true;
  std::int64_t first_audio_timestamp = 0;

  while (true) {
    DWORD stream_index = 0;
    DWORD flags = 0;
    std::int64_t timestamp = 0;
    wil::com_ptr<IMFSample> sample;

    hr = reader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &stream_index, &flags, &timestamp,
                            sample.put());

    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
      break;
    }

    if ((flags & MF_SOURCE_READERF_STREAMTICK) || !sample) {
      continue;
    }

    bool is_video = (stream_index == MF_SOURCE_READER_FIRST_VIDEO_STREAM) || (stream_index == 0);
    DWORD out_index;

    if (is_video) {
      if (first_video_sample) {
        first_video_timestamp = timestamp;
        first_video_sample = false;
      }
      std::int64_t adjusted_ts = timestamp - first_video_timestamp;
      sample->SetSampleTime(adjusted_ts);
      out_index = video_out_index;
    } else {
      if (!has_audio) continue;
      if (first_audio_sample) {
        first_audio_timestamp = timestamp;
        first_audio_sample = false;
      }
      std::int64_t adjusted_ts = timestamp - first_audio_timestamp;
      sample->SetSampleTime(adjusted_ts);
      out_index = audio_out_index;
    }

    hr = sink_writer->WriteSample(out_index, sample.get());
    if (FAILED(hr)) {
      return std::unexpected(std::format("WriteSample failed: {:08X}", static_cast<uint32_t>(hr)));
    }
  }

  // 6. Finalize
  hr = sink_writer->Finalize();
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to finalize output: {:08X}", static_cast<uint32_t>(hr)));
  }

  Logger().info("Scaled video saved: {}", output_path.string());
  return {};
}

}  // namespace Features::ReplayBuffer::Trimmer
