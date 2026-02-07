module;

#include <codecapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

module Features.ReplayBuffer.Trimmer;

import std;
import Utils.Logger;
import <mfapi.h>;
import <wil/com.h>;
import <windows.h>;
import <Mferror.h>;

namespace Features::ReplayBuffer::Trimmer {

// 获取文件的视频时长（100ns 单位）
auto get_file_duration(const std::filesystem::path& path)
    -> std::expected<std::int64_t, std::string> {
  wil::com_ptr<IMFSourceReader> reader;
  HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, reader.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create source reader: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 获取视频时长
  PROPVARIANT var;
  PropVariantInit(&var);
  hr = reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
  if (FAILED(hr)) {
    return std::unexpected("Failed to get duration");
  }

  std::int64_t duration = var.hVal.QuadPart;
  PropVariantClear(&var);
  return duration;
}

// 处理单个输入文件，写入到 sink_writer
auto process_input_file(const std::filesystem::path& input_path, IMFSinkWriter* sink_writer,
                        DWORD video_out_index, DWORD audio_out_index, bool has_audio,
                        std::int64_t seek_position_100ns, std::int64_t& time_offset_100ns)
    -> std::expected<void, std::string> {
  // 创建 SourceReader
  wil::com_ptr<IMFAttributes> reader_attrs;
  MFCreateAttributes(reader_attrs.put(), 1);
  // 不解码，直接读取压缩样本
  reader_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);

  wil::com_ptr<IMFSourceReader> reader;
  HRESULT hr = MFCreateSourceReaderFromURL(input_path.c_str(), reader_attrs.get(), reader.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to open {}: {:08X}", input_path.string(), static_cast<uint32_t>(hr)));
  }

  // 设置为压缩样本输出（stream copy 模式）
  // 视频流
  wil::com_ptr<IMFMediaType> video_native_type;
  hr = reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, video_native_type.put());
  if (SUCCEEDED(hr)) {
    reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr,
                                video_native_type.get());
  }

  // 音频流
  if (has_audio) {
    wil::com_ptr<IMFMediaType> audio_native_type;
    hr =
        reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, audio_native_type.put());
    if (SUCCEEDED(hr)) {
      reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr,
                                  audio_native_type.get());
    }
  }

  // Seek 到指定位置
  if (seek_position_100ns > 0) {
    PROPVARIANT seek_var;
    PropVariantInit(&seek_var);
    seek_var.vt = VT_I8;
    seek_var.hVal.QuadPart = seek_position_100ns;
    hr = reader->SetCurrentPosition(GUID_NULL, seek_var);
    PropVariantClear(&seek_var);
    if (FAILED(hr)) {
      Logger().warn("Seek failed, reading from beginning: {:08X}", static_cast<uint32_t>(hr));
    }
  }

  // 记录第一个样本的时间戳用于偏移计算
  bool first_video_sample = true;
  std::int64_t first_video_timestamp = 0;
  bool first_audio_sample = true;
  std::int64_t first_audio_timestamp = 0;

  // 读取并写入样本
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

    if (flags & MF_SOURCE_READERF_STREAMTICK) {
      continue;
    }

    if (!sample) {
      continue;
    }

    // 判断是视频还是音频
    bool is_video = (stream_index == MF_SOURCE_READER_FIRST_VIDEO_STREAM) || (stream_index == 0);
    DWORD out_index;

    if (is_video) {
      if (first_video_sample) {
        first_video_timestamp = timestamp;
        first_video_sample = false;
      }
      // 调整时间戳：相对于文件内的偏移 + 全局累计偏移
      std::int64_t adjusted_ts = (timestamp - first_video_timestamp) + time_offset_100ns;
      sample->SetSampleTime(adjusted_ts);
      out_index = video_out_index;
    } else {
      if (!has_audio) continue;
      if (first_audio_sample) {
        first_audio_timestamp = timestamp;
        first_audio_sample = false;
      }
      std::int64_t adjusted_ts = (timestamp - first_audio_timestamp) + time_offset_100ns;
      sample->SetSampleTime(adjusted_ts);
      out_index = audio_out_index;
    }

    hr = sink_writer->WriteSample(out_index, sample.get());
    if (FAILED(hr)) {
      Logger().error("WriteSample failed: {:08X}, aborting file", static_cast<uint32_t>(hr));
      return std::unexpected(std::format("WriteSample failed: {:08X}", static_cast<uint32_t>(hr)));
    }
  }

  return {};
}

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

// 处理单个输入文件（转码模式）
auto process_input_file_transcode(const std::filesystem::path& input_path,
                                  IMFSinkWriter* sink_writer, DWORD video_out_index,
                                  DWORD audio_out_index, bool has_audio,
                                  std::int64_t seek_position_100ns, std::int64_t& time_offset_100ns,
                                  std::uint32_t target_width, std::uint32_t target_height)
    -> std::expected<void, std::string> {
  Logger().debug("[Transcode] Processing file: {}", input_path.string());
  Logger().debug("[Transcode] Target size: {}x{}, seek: {}", target_width, target_height,
                 seek_position_100ns);

  // 创建 SourceReader，启用解码
  wil::com_ptr<IMFAttributes> reader_attrs;
  MFCreateAttributes(reader_attrs.put(), 2);
  reader_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
  // ADVANCED 版本支持解码 + 缩放 + 格式转换（普通版本只支持格式转换）
  reader_attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);

  wil::com_ptr<IMFSourceReader> reader;
  HRESULT hr = MFCreateSourceReaderFromURL(input_path.c_str(), reader_attrs.get(), reader.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to open {}: {:08X}", input_path.string(), static_cast<uint32_t>(hr)));
  }

  // 设置视频输出为 NV12（解码后）—— NV12 是视频编解码的原生格式，
  // 避免 RGB32 的 bottom-up stride 翻转问题，且性能更好
  wil::com_ptr<IMFMediaType> video_decode_type;
  MFCreateMediaType(video_decode_type.put());
  video_decode_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  video_decode_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  MFSetAttributeSize(video_decode_type.get(), MF_MT_FRAME_SIZE, target_width, target_height);
  hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr,
                                   video_decode_type.get());
  if (FAILED(hr)) {
    Logger().debug("[Transcode] SetCurrentMediaType NV12 failed: {:08X}, trying RGB32",
                   static_cast<uint32_t>(hr));
    // Fallback 到 RGB32
    video_decode_type = nullptr;
    MFCreateMediaType(video_decode_type.put());
    video_decode_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    video_decode_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    MFSetAttributeSize(video_decode_type.get(), MF_MT_FRAME_SIZE, target_width, target_height);
    hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr,
                                     video_decode_type.get());
    if (FAILED(hr)) {
      return std::unexpected(
          std::format("Failed to set video decode type: {:08X}", static_cast<uint32_t>(hr)));
    }
  }

  // 获取实际的解码输出格式
  wil::com_ptr<IMFMediaType> actual_video_type;
  hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, actual_video_type.put());
  GUID actual_subtype = GUID_NULL;
  UINT32 actual_w = 0, actual_h = 0;
  if (SUCCEEDED(hr)) {
    MFGetAttributeSize(actual_video_type.get(), MF_MT_FRAME_SIZE, &actual_w, &actual_h);
    actual_video_type->GetGUID(MF_MT_SUBTYPE, &actual_subtype);
    const char* format_name = (actual_subtype == MFVideoFormat_NV12) ? "NV12" : "RGB32";
    Logger().debug("[Transcode] Actual decoder output: {}x{}, format={}", actual_w, actual_h,
                   format_name);
  }

  // 音频输出为 PCM
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

  // Seek
  if (seek_position_100ns > 0) {
    PROPVARIANT seek_var;
    PropVariantInit(&seek_var);
    seek_var.vt = VT_I8;
    seek_var.hVal.QuadPart = seek_position_100ns;
    reader->SetCurrentPosition(GUID_NULL, seek_var);
    PropVariantClear(&seek_var);
  }

  // 读取并写入
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

        // 调试：输出第一个视频样本的详细信息
        wil::com_ptr<IMFMediaBuffer> dbg_buffer;
        if (SUCCEEDED(sample->GetBufferByIndex(0, dbg_buffer.put()))) {
          DWORD buf_len = 0, max_len = 0;
          dbg_buffer->GetCurrentLength(&buf_len);
          dbg_buffer->GetMaxLength(&max_len);
          // NV12: width * height * 1.5
          Logger().debug(
              "[Transcode] First video sample: buffer_len={}, max_len={}, expected_nv12={} "
              "({}x{}x1.5)",
              buf_len, max_len, target_width * target_height * 3 / 2, target_width, target_height);
        }
      }
      std::int64_t adjusted_ts = (timestamp - first_video_timestamp) + time_offset_100ns;
      sample->SetSampleTime(adjusted_ts);
      out_index = video_out_index;
    } else {
      if (!has_audio) continue;
      if (first_audio_sample) {
        first_audio_timestamp = timestamp;
        first_audio_sample = false;
      }
      std::int64_t adjusted_ts = (timestamp - first_audio_timestamp) + time_offset_100ns;
      sample->SetSampleTime(adjusted_ts);
      out_index = audio_out_index;
    }

    hr = sink_writer->WriteSample(out_index, sample.get());
    if (FAILED(hr)) {
      // 获取更多调试信息
      if (is_video) {
        wil::com_ptr<IMFMediaBuffer> err_buffer;
        if (SUCCEEDED(sample->GetBufferByIndex(0, err_buffer.put()))) {
          DWORD buf_len = 0;
          err_buffer->GetCurrentLength(&buf_len);
          Logger().error(
              "[Transcode] WriteSample failed: {:08X}, is_video={}, buffer_len={}, "
              "expected_nv12={}",
              static_cast<uint32_t>(hr), is_video, buf_len, target_width * target_height * 3 / 2);
        }
      } else {
        Logger().error("[Transcode] WriteSample (audio) failed: {:08X}", static_cast<uint32_t>(hr));
      }
      return std::unexpected(std::format("WriteSample failed: {:08X}", static_cast<uint32_t>(hr)));
    }
  }

  return {};
}

auto trim_and_concat(const std::vector<std::filesystem::path>& input_paths,
                     const std::filesystem::path& output_path, double duration_seconds,
                     std::optional<ScaleConfig> scale_config) -> std::expected<void, std::string> {
  if (input_paths.empty()) {
    return std::unexpected("No input files provided");
  }

  // 1. 计算每个文件的时长，决定从哪个文件的什么位置开始
  std::vector<std::int64_t> durations;
  std::int64_t total_duration_100ns = 0;

  for (const auto& path : input_paths) {
    auto dur = get_file_duration(path);
    if (!dur) {
      Logger().warn("Failed to get duration for {}: {}", path.string(), dur.error());
      durations.push_back(0);
      continue;
    }
    durations.push_back(*dur);
    total_duration_100ns += *dur;
  }

  std::int64_t target_duration_100ns = static_cast<std::int64_t>(duration_seconds * 10'000'000);

  // 计算 seek 位置：从总时长末尾倒推 target_duration
  std::int64_t skip_duration = total_duration_100ns - target_duration_100ns;
  if (skip_duration < 0) skip_duration = 0;

  // 找到起始文件和 seek 偏移
  size_t start_file_index = 0;
  std::int64_t seek_in_first_file = 0;
  {
    std::int64_t accumulated = 0;
    for (size_t i = 0; i < input_paths.size(); i++) {
      if (accumulated + durations[i] > skip_duration) {
        start_file_index = i;
        seek_in_first_file = skip_duration - accumulated;
        break;
      }
      accumulated += durations[i];
    }
  }

  // 2. 创建 SinkWriter
  // 先从第一个需要处理的文件获取媒体类型
  wil::com_ptr<IMFSourceReader> probe_reader;
  HRESULT hr = MFCreateSourceReaderFromURL(input_paths[start_file_index].c_str(), nullptr,
                                           probe_reader.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to open first input for probing");
  }

  wil::com_ptr<IMFMediaType> video_type;
  hr = probe_reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, video_type.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to get video media type");
  }

  // 获取源视频尺寸
  UINT32 src_width = 0, src_height = 0;
  MFGetAttributeSize(video_type.get(), MF_MT_FRAME_SIZE, &src_width, &src_height);

  // 确定是否需要缩放
  bool need_transcode = false;
  std::uint32_t target_width = src_width;
  std::uint32_t target_height = src_height;

  if (scale_config && scale_config->target_short_edge > 0) {
    auto [new_w, new_h] =
        calculate_scaled_dimensions(src_width, src_height, scale_config->target_short_edge);
    if (new_w != src_width || new_h != src_height) {
      need_transcode = true;
      target_width = new_w;
      target_height = new_h;
      Logger().info("Scaling video from {}x{} to {}x{}", src_width, src_height, target_width,
                    target_height);
    }
  }

  // 多段落拼接时强制转码：不同编码器实例生成的 H.264 段落具有不同的 SPS/PPS，
  // stream copy 模式下 SinkWriter 只识别第一个段落的 codec private data，
  // 后续段落的压缩样本会被拒绝 (E_UNEXPECTED)。转码模式先解码再编码，避免此问题。
  size_t files_to_process = input_paths.size() - start_file_index;
  if (!need_transcode && files_to_process > 1) {
    need_transcode = true;
    // 不缩放，使用源分辨率转码
    Logger().info("Multi-segment concat: forcing transcode mode for {} files", files_to_process);
    if (!scale_config) {
      // 从源文件获取帧率和码率
      UINT32 fps_num = 0, fps_den = 1;
      MFGetAttributeRatio(video_type.get(), MF_MT_FRAME_RATE, &fps_num, &fps_den);
      UINT32 bitrate = 0;
      video_type->GetUINT32(MF_MT_AVG_BITRATE, &bitrate);
      ScaleConfig auto_config;
      auto_config.target_short_edge = 0;
      auto_config.fps = (fps_num > 0) ? fps_num : 30;
      auto_config.bitrate = (bitrate > 0) ? bitrate : 20'000'000;
      scale_config = auto_config;
      Logger().debug("[Transcode] Auto config: fps={}, bitrate={}", auto_config.fps,
                     auto_config.bitrate);
    }
  }

  Logger().debug("[Transcode] Source video: {}x{}, need_transcode={}, target={}x{}", src_width,
                 src_height, need_transcode, target_width, target_height);

  // 检查是否有音频
  bool has_audio = false;
  wil::com_ptr<IMFMediaType> audio_type;
  hr = probe_reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, audio_type.put());
  if (SUCCEEDED(hr)) {
    has_audio = true;
  }

  probe_reader = nullptr;  // 释放

  // 创建 SinkWriter（启用硬件变换以支持自动缩放）
  wil::com_ptr<IMFAttributes> sink_attrs;
  MFCreateAttributes(sink_attrs.put(), 1);
  sink_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

  wil::com_ptr<IMFSinkWriter> sink_writer;
  hr = MFCreateSinkWriterFromURL(output_path.c_str(), nullptr, sink_attrs.get(), sink_writer.put());
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to create sink writer: {:08X}", static_cast<uint32_t>(hr)));
  }

  DWORD video_out_index = 0;
  DWORD audio_out_index = 0;

  if (need_transcode) {
    Logger().debug("[Transcode] Setting up SinkWriter for transcode mode");

    // 转码模式：设置输出为 H.264
    wil::com_ptr<IMFMediaType> output_video_type;
    MFCreateMediaType(output_video_type.put());
    output_video_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    output_video_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    output_video_type->SetUINT32(MF_MT_AVG_BITRATE, scale_config->bitrate);
    MFSetAttributeSize(output_video_type.get(), MF_MT_FRAME_SIZE, target_width, target_height);
    MFSetAttributeRatio(output_video_type.get(), MF_MT_FRAME_RATE, scale_config->fps, 1);
    MFSetAttributeRatio(output_video_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    output_video_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    Logger().debug("[Transcode] SinkWriter output: H.264 {}x{} @ {}fps, {} bps", target_width,
                   target_height, scale_config->fps, scale_config->bitrate);

    hr = sink_writer->AddStream(output_video_type.get(), &video_out_index);
    if (FAILED(hr)) {
      return std::unexpected("Failed to add video stream for transcoding");
    }

    // 输入类型为 NV12（目标分辨率，SourceReader 已通过 ADVANCED_VIDEO_PROCESSING 完成缩放）
    wil::com_ptr<IMFMediaType> input_video_type;
    MFCreateMediaType(input_video_type.put());
    input_video_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input_video_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(input_video_type.get(), MF_MT_FRAME_SIZE, target_width, target_height);
    MFSetAttributeRatio(input_video_type.get(), MF_MT_FRAME_RATE, scale_config->fps, 1);
    MFSetAttributeRatio(input_video_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    input_video_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    // NV12 buffer size = width * height * 1.5
    Logger().debug("[Transcode] SinkWriter input: NV12 {}x{}, expected buffer={} bytes",
                   target_width, target_height, target_width * target_height * 3 / 2);

    hr = sink_writer->SetInputMediaType(video_out_index, input_video_type.get(), nullptr);
    if (FAILED(hr)) {
      return std::unexpected(std::format("Failed to set video input type for transcoding: {:08X}",
                                         static_cast<uint32_t>(hr)));
    }

    // 音频转码为 AAC
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
        Logger().warn("Failed to add audio stream for transcoding");
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
          Logger().warn("Failed to set audio input type for transcoding");
          has_audio = false;
        }
      }
    }
  } else {
    // Stream Copy 模式
    hr = sink_writer->AddStream(video_type.get(), &video_out_index);
    if (FAILED(hr)) {
      return std::unexpected("Failed to add video stream to sink writer");
    }

    hr = sink_writer->SetInputMediaType(video_out_index, video_type.get(), nullptr);
    if (FAILED(hr)) {
      return std::unexpected("Failed to set video input media type");
    }

    if (has_audio) {
      hr = sink_writer->AddStream(audio_type.get(), &audio_out_index);
      if (FAILED(hr)) {
        Logger().warn("Failed to add audio stream, continuing without audio");
        has_audio = false;
      } else {
        hr = sink_writer->SetInputMediaType(audio_out_index, audio_type.get(), nullptr);
        if (FAILED(hr)) {
          Logger().warn("Failed to set audio input type, continuing without audio");
          has_audio = false;
        }
      }
    }
  }

  // 开始写入
  hr = sink_writer->BeginWriting();
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to begin writing: {:08X}", static_cast<uint32_t>(hr)));
  }

  // 3. 处理每个输入文件
  std::int64_t time_offset = 0;
  for (size_t i = start_file_index; i < input_paths.size(); i++) {
    std::int64_t seek_pos = (i == start_file_index) ? seek_in_first_file : 0;

    std::expected<void, std::string> result;
    if (need_transcode) {
      result = process_input_file_transcode(input_paths[i], sink_writer.get(), video_out_index,
                                            audio_out_index, has_audio, seek_pos, time_offset,
                                            target_width, target_height);
    } else {
      result = process_input_file(input_paths[i], sink_writer.get(), video_out_index,
                                  audio_out_index, has_audio, seek_pos, time_offset);
    }
    if (!result) {
      Logger().warn("Error processing {}: {}", input_paths[i].string(), result.error());
    }

    // 累加已处理的时长
    if (i == start_file_index) {
      time_offset += durations[i] - seek_in_first_file;
    } else {
      time_offset += durations[i];
    }
  }

  // 4. Finalize
  hr = sink_writer->Finalize();
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("Failed to finalize output: {:08X}", static_cast<uint32_t>(hr)));
  }

  Logger().info("Trimmed video saved: {}", output_path.string());
  return {};
}

}  // namespace Features::ReplayBuffer::Trimmer
