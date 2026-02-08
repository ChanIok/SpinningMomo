module;

#include <codecapi.h>
#include <d3d11.h>
#include <mferror.h>
#include <mfidl.h>
#include <propvarutil.h>

module Utils.Media.VideoScaler;

import std;
import Utils.Logger;
import <mfapi.h>;
import <wil/com.h>;

namespace Utils::Media::VideoScaler {

// 计算缩放后的分辨率
auto calculate_scaled_dimensions(std::uint32_t src_width, std::uint32_t src_height,
                                 const ScaleConfig& config)
    -> std::pair<std::uint32_t, std::uint32_t> {
  // 如果指定了短边目标
  if (config.target_short_edge > 0) {
    bool width_is_shorter = src_width <= src_height;
    std::uint32_t short_edge = width_is_shorter ? src_width : src_height;
    std::uint32_t long_edge = width_is_shorter ? src_height : src_width;

    double scale = static_cast<double>(config.target_short_edge) / short_edge;
    std::uint32_t new_short = config.target_short_edge;
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

  // 如果指定了具体宽高
  if (config.target_width > 0 && config.target_height > 0) {
    return {(config.target_width / 2) * 2, (config.target_height / 2) * 2};
  }

  // 默认不缩放
  return {src_width, src_height};
}

// 从 MediaSource 获取视频分辨率
auto get_source_video_dimensions(IMFMediaSource* source)
    -> std::expected<std::pair<std::uint32_t, std::uint32_t>, std::string> {
  wil::com_ptr<IMFPresentationDescriptor> pd;
  HRESULT hr = source->CreatePresentationDescriptor(pd.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create presentation descriptor");
  }

  DWORD stream_count = 0;
  pd->GetStreamDescriptorCount(&stream_count);

  for (DWORD i = 0; i < stream_count; ++i) {
    BOOL selected = FALSE;
    wil::com_ptr<IMFStreamDescriptor> sd;
    hr = pd->GetStreamDescriptorByIndex(i, &selected, sd.put());
    if (FAILED(hr)) continue;

    wil::com_ptr<IMFMediaTypeHandler> handler;
    hr = sd->GetMediaTypeHandler(handler.put());
    if (FAILED(hr)) continue;

    GUID major_type;
    hr = handler->GetMajorType(&major_type);
    if (FAILED(hr) || major_type != MFMediaType_Video) continue;

    wil::com_ptr<IMFMediaType> media_type;
    hr = handler->GetCurrentMediaType(media_type.put());
    if (FAILED(hr)) continue;

    UINT32 width = 0, height = 0;
    hr = MFGetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, &width, &height);
    if (SUCCEEDED(hr) && width > 0 && height > 0) {
      return std::make_pair(width, height);
    }
  }

  return std::unexpected("No video stream found in source");
}

auto scale_video_file(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path, const ScaleConfig& config)
    -> std::expected<ScaleResult, std::string> {
  ScaleResult result = {};

  // 1. 创建 MediaSource
  wil::com_ptr<IMFSourceResolver> resolver;
  HRESULT hr = MFCreateSourceResolver(resolver.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create source resolver");
  }

  MF_OBJECT_TYPE object_type = MF_OBJECT_INVALID;
  wil::com_ptr<IUnknown> source_unk;
  hr = resolver->CreateObjectFromURL(input_path.c_str(), MF_RESOLUTION_MEDIASOURCE, nullptr,
                                     &object_type, source_unk.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create media source: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  wil::com_ptr<IMFMediaSource> media_source;
  hr = source_unk->QueryInterface(IID_PPV_ARGS(media_source.put()));
  if (FAILED(hr)) {
    return std::unexpected("Failed to get IMFMediaSource interface");
  }

  // 2. 获取源视频分辨率
  auto dims_result = get_source_video_dimensions(media_source.get());
  if (!dims_result) {
    return std::unexpected(dims_result.error());
  }
  auto [src_width, src_height] = *dims_result;

  result.src_width = src_width;
  result.src_height = src_height;

  // 3. 计算目标分辨率
  auto [target_width, target_height] = calculate_scaled_dimensions(src_width, src_height, config);
  result.target_width = target_width;
  result.target_height = target_height;

  // 检查是否需要缩放
  if (target_width == src_width && target_height == src_height) {
    Logger().info("Video already at target resolution {}x{}, skipping scale", src_width,
                  src_height);
    result.scaled = false;
    media_source->Shutdown();
    return result;
  }

  Logger().info("Scaling video from {}x{} to {}x{} using Transcode API", src_width, src_height,
                target_width, target_height);

  // 4. 创建 TranscodeProfile
  wil::com_ptr<IMFTranscodeProfile> profile;
  hr = MFCreateTranscodeProfile(profile.put());
  if (FAILED(hr)) {
    media_source->Shutdown();
    return std::unexpected("Failed to create transcode profile");
  }

  // 视频属性
  wil::com_ptr<IMFAttributes> video_attrs;
  MFCreateAttributes(video_attrs.put(), 10);

  // 根据 codec 配置选择编码器
  if (config.codec == VideoCodec::H265) {
    video_attrs->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_HEVC);
    video_attrs->SetUINT32(MF_MT_VIDEO_PROFILE, eAVEncH265VProfile_Main_420_8);
    Logger().debug("Using H.265/HEVC codec");
  } else {
    video_attrs->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    video_attrs->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
    Logger().debug("Using H.264/AVC codec");
  }

  MFSetAttributeSize(video_attrs.get(), MF_MT_FRAME_SIZE, target_width, target_height);
  MFSetAttributeRatio(video_attrs.get(), MF_MT_FRAME_RATE, config.fps, 1);
  video_attrs->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

  // 根据 rate_control 配置码率控制
  if (config.rate_control == RateControl::CBR) {
    // CBR: 使用指定码率
    video_attrs->SetUINT32(MF_MT_AVG_BITRATE, config.bitrate);
    Logger().debug("Using CBR with bitrate: {} bps", config.bitrate);
  } else {
    // VBR: 使用质量参数，码率作为上限
    video_attrs->SetUINT32(MF_MT_AVG_BITRATE, config.bitrate);
    Logger().debug("Using VBR with quality: {}, max bitrate: {} bps", config.quality,
                   config.bitrate);
  }

  // 质量 vs 速度：0=最快，100=最高质量
  video_attrs->SetUINT32(MF_TRANSCODE_QUALITYVSSPEED, config.quality);

  hr = profile->SetVideoAttributes(video_attrs.get());
  if (FAILED(hr)) {
    media_source->Shutdown();
    return std::unexpected("Failed to set video attributes");
  }

  // 音频属性（MF_MT_AUDIO_AVG_BYTES_PER_SECOND 为字节/秒 = bitrate/8）
  std::uint32_t audio_bytes_per_sec = config.audio_bitrate / 8;
  wil::com_ptr<IMFAttributes> audio_attrs;
  MFCreateAttributes(audio_attrs.put(), 6);
  audio_attrs->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
  audio_attrs->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
  audio_attrs->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
  audio_attrs->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
  audio_attrs->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
  audio_attrs->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, audio_bytes_per_sec);

  hr = profile->SetAudioAttributes(audio_attrs.get());
  if (FAILED(hr)) {
    Logger().warn("Failed to set audio attributes, continuing without audio");
  }

  // 容器属性
  wil::com_ptr<IMFAttributes> container_attrs;
  MFCreateAttributes(container_attrs.put(), 2);
  container_attrs->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
  // 允许硬件加速
  container_attrs->SetUINT32(MF_TRANSCODE_TOPOLOGYMODE, MF_TRANSCODE_TOPOLOGYMODE_HARDWARE_ALLOWED);

  hr = profile->SetContainerAttributes(container_attrs.get());
  if (FAILED(hr)) {
    media_source->Shutdown();
    return std::unexpected("Failed to set container attributes");
  }

  // 5. 创建 Transcode Topology
  wil::com_ptr<IMFTopology> topology;
  hr = MFCreateTranscodeTopology(media_source.get(), output_path.c_str(), profile.get(),
                                 topology.put());
  if (FAILED(hr)) {
    media_source->Shutdown();
    return std::unexpected("Failed to create transcode topology: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  Logger().debug("Transcode topology created");

  // 6. 创建 MediaSession
  wil::com_ptr<IMFMediaSession> session;
  hr = MFCreateMediaSession(nullptr, session.put());
  if (FAILED(hr)) {
    media_source->Shutdown();
    return std::unexpected("Failed to create media session");
  }

  // 设置 topology
  hr = session->SetTopology(0, topology.get());
  if (FAILED(hr)) {
    session->Shutdown();
    media_source->Shutdown();
    return std::unexpected("Failed to set topology: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  // 7. 启动 session
  PROPVARIANT var_start;
  PropVariantInit(&var_start);
  hr = session->Start(&GUID_NULL, &var_start);
  PropVariantClear(&var_start);
  if (FAILED(hr)) {
    session->Shutdown();
    media_source->Shutdown();
    return std::unexpected("Failed to start session: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  Logger().debug("Transcode session started");

  // 8. 等待完成（同步轮询）
  bool transcoding = true;
  while (transcoding) {
    wil::com_ptr<IMFMediaEvent> event;
    hr = session->GetEvent(0, event.put());  // 阻塞等待
    if (FAILED(hr)) {
      Logger().warn("GetEvent failed: {:08X}", static_cast<std::uint32_t>(hr));
      break;
    }

    MediaEventType event_type = MEUnknown;
    event->GetType(&event_type);

    HRESULT event_status = S_OK;
    event->GetStatus(&event_status);

    switch (event_type) {
      case MESessionTopologySet:
        Logger().debug("Topology set");
        break;

      case MESessionStarted:
        Logger().debug("Session started");
        break;

      case MESessionEnded:
        Logger().debug("Session ended, closing...");
        session->Close();
        break;

      case MESessionClosed:
        Logger().debug("Session closed");
        transcoding = false;
        break;

      case MEError:
        Logger().error("Session error: {:08X}", static_cast<std::uint32_t>(event_status));
        transcoding = false;
        break;

      default:
        // 其他事件忽略
        break;
    }

    if (FAILED(event_status)) {
      Logger().error("Event status failed: {:08X}", static_cast<std::uint32_t>(event_status));
      session->Close();
      // 继续循环等待 MESessionClosed
    }
  }

  // 9. 清理
  session->Shutdown();
  media_source->Shutdown();

  // 检查输出文件
  std::error_code ec;
  auto file_size = std::filesystem::file_size(output_path, ec);
  if (ec || file_size == 0) {
    return std::unexpected("Output file is empty or not created");
  }

  result.scaled = true;
  Logger().info("Video scaling completed using Transcode API, saved to {}", output_path.string());
  return result;
}

}  // namespace Utils::Media::VideoScaler
