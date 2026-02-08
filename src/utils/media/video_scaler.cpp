module;

#include <codecapi.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

module Utils.Media.VideoScaler;

import std;
import Utils.Graphics.D3D;
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

// D3D11 Video Processor 缩放上下文
struct VideoProcessorContext {
  wil::com_ptr<ID3D11VideoDevice> video_device;
  wil::com_ptr<ID3D11VideoContext> video_context;
  wil::com_ptr<ID3D11VideoProcessorEnumerator> vp_enum;
  wil::com_ptr<ID3D11VideoProcessor> video_processor;
  wil::com_ptr<ID3D11Texture2D> output_texture;  // 缩放后的 NV12 纹理
  std::uint32_t src_width = 0;
  std::uint32_t src_height = 0;
  std::uint32_t dst_width = 0;
  std::uint32_t dst_height = 0;
};

// 创建 D3D11 Video Processor（支持缩放）
auto create_video_processor_for_scaling(ID3D11Device* device, std::uint32_t src_width,
                                        std::uint32_t src_height, std::uint32_t dst_width,
                                        std::uint32_t dst_height)
    -> std::expected<VideoProcessorContext, std::string> {
  VideoProcessorContext ctx;
  ctx.src_width = src_width;
  ctx.src_height = src_height;
  ctx.dst_width = dst_width;
  ctx.dst_height = dst_height;

  // 获取 Video Device 接口
  HRESULT hr = device->QueryInterface(IID_PPV_ARGS(ctx.video_device.put()));
  if (FAILED(hr)) {
    return std::unexpected("Failed to get ID3D11VideoDevice");
  }

  // 获取 Video Context 接口
  wil::com_ptr<ID3D11DeviceContext> d3d_context;
  device->GetImmediateContext(d3d_context.put());
  hr = d3d_context->QueryInterface(IID_PPV_ARGS(ctx.video_context.put()));
  if (FAILED(hr)) {
    return std::unexpected("Failed to get ID3D11VideoContext");
  }

  // 创建 Video Processor Enumerator（关键：输入输出尺寸不同）
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
  content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  content_desc.InputWidth = src_width;
  content_desc.InputHeight = src_height;
  content_desc.OutputWidth = dst_width;
  content_desc.OutputHeight = dst_height;
  content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  hr = ctx.video_device->CreateVideoProcessorEnumerator(&content_desc, ctx.vp_enum.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create video processor enumerator: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  // 创建 Video Processor
  hr = ctx.video_device->CreateVideoProcessor(ctx.vp_enum.get(), 0, ctx.video_processor.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create video processor");
  }

  // 设置源矩形（输入完整帧）
  RECT src_rect = {0, 0, static_cast<LONG>(src_width), static_cast<LONG>(src_height)};
  ctx.video_context->VideoProcessorSetStreamSourceRect(ctx.video_processor.get(), 0, TRUE,
                                                       &src_rect);

  // 设置目标矩形（输出完整帧）
  RECT dst_rect = {0, 0, static_cast<LONG>(dst_width), static_cast<LONG>(dst_height)};
  ctx.video_context->VideoProcessorSetStreamDestRect(ctx.video_processor.get(), 0, TRUE, &dst_rect);
  ctx.video_context->VideoProcessorSetOutputTargetRect(ctx.video_processor.get(), TRUE, &dst_rect);

  // 禁用自动处理（保持质量）
  ctx.video_context->VideoProcessorSetStreamAutoProcessingMode(ctx.video_processor.get(), 0, FALSE);

  // 创建输出纹理（NV12 格式，用于编码器输入）
  D3D11_TEXTURE2D_DESC output_desc = {};
  output_desc.Width = dst_width;
  output_desc.Height = dst_height;
  output_desc.MipLevels = 1;
  output_desc.ArraySize = 1;
  output_desc.Format = DXGI_FORMAT_NV12;
  output_desc.SampleDesc.Count = 1;
  output_desc.Usage = D3D11_USAGE_DEFAULT;
  output_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_VIDEO_ENCODER;

  hr = device->CreateTexture2D(&output_desc, nullptr, ctx.output_texture.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create output NV12 texture");
  }

  Logger().debug("Video Processor created for scaling: {}x{} -> {}x{}", src_width, src_height,
                 dst_width, dst_height);
  return ctx;
}

// 执行缩放操作
auto scale_frame(VideoProcessorContext& ctx, ID3D11Texture2D* input_texture) -> HRESULT {
  // 创建输入视图
  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {};
  input_view_desc.FourCC = 0;
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.MipSlice = 0;

  wil::com_ptr<ID3D11VideoProcessorInputView> input_view;
  HRESULT hr = ctx.video_device->CreateVideoProcessorInputView(input_texture, ctx.vp_enum.get(),
                                                               &input_view_desc, input_view.put());
  if (FAILED(hr)) return hr;

  // 创建输出视图
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {};
  output_view_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  output_view_desc.Texture2D.MipSlice = 0;

  wil::com_ptr<ID3D11VideoProcessorOutputView> output_view;
  hr = ctx.video_device->CreateVideoProcessorOutputView(ctx.output_texture.get(), ctx.vp_enum.get(),
                                                        &output_view_desc, output_view.put());
  if (FAILED(hr)) return hr;

  // 执行缩放
  D3D11_VIDEO_PROCESSOR_STREAM stream = {};
  stream.Enable = TRUE;
  stream.OutputIndex = 0;
  stream.InputFrameOrField = 0;
  stream.pInputSurface = input_view.get();

  return ctx.video_context->VideoProcessorBlt(ctx.video_processor.get(), output_view.get(), 0, 1,
                                              &stream);
}

auto scale_video_file(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path, const ScaleConfig& config)
    -> std::expected<ScaleResult, std::string> {
  ScaleResult result = {};

  // 1. 创建 D3D11 设备
  auto d3d_result = Utils::Graphics::D3D::create_headless_d3d_device();
  if (!d3d_result) {
    return std::unexpected("Failed to create D3D11 device: " + d3d_result.error());
  }
  auto [device, context] = std::move(*d3d_result);

  // 启用多线程保护
  wil::com_ptr<ID3D11Multithread> multithread;
  if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(multithread.put())))) {
    multithread->SetMultithreadProtected(TRUE);
  }

  // 2. 创建 DXGI Device Manager
  wil::com_ptr<IMFDXGIDeviceManager> dxgi_manager;
  UINT reset_token = 0;
  HRESULT hr = MFCreateDXGIDeviceManager(&reset_token, dxgi_manager.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create DXGI Device Manager");
  }

  hr = dxgi_manager->ResetDevice(device.get(), reset_token);
  if (FAILED(hr)) {
    return std::unexpected("Failed to reset DXGI device");
  }

  // 3. 创建 SourceReader（配置 D3D11 加速）
  wil::com_ptr<IMFAttributes> reader_attrs;
  MFCreateAttributes(reader_attrs.put(), 3);
  reader_attrs->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, dxgi_manager.get());
  reader_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
  reader_attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);

  wil::com_ptr<IMFSourceReader> reader;
  hr = MFCreateSourceReaderFromURL(input_path.c_str(), reader_attrs.get(), reader.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create source reader: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  // 获取源视频格式
  wil::com_ptr<IMFMediaType> native_type;
  hr = reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, native_type.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to get native video type");
  }

  UINT32 src_width = 0, src_height = 0;
  MFGetAttributeSize(native_type.get(), MF_MT_FRAME_SIZE, &src_width, &src_height);

  result.src_width = src_width;
  result.src_height = src_height;

  // 计算目标分辨率
  auto [target_width, target_height] = calculate_scaled_dimensions(src_width, src_height, config);
  result.target_width = target_width;
  result.target_height = target_height;

  // 检查是否需要缩放
  if (target_width == src_width && target_height == src_height) {
    Logger().info("Video already at target resolution {}x{}, skipping scale", src_width,
                  src_height);
    result.scaled = false;
    return result;
  }

  Logger().info("Scaling video from {}x{} to {}x{}", src_width, src_height, target_width,
                target_height);

  // 配置 SourceReader 输出 NV12（D3D11 Video Processor 优化格式）
  wil::com_ptr<IMFMediaType> decoder_output_type;
  MFCreateMediaType(decoder_output_type.put());
  decoder_output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  decoder_output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  MFSetAttributeSize(decoder_output_type.get(), MF_MT_FRAME_SIZE, src_width, src_height);

  hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr,
                                   decoder_output_type.get());
  if (FAILED(hr)) {
    return std::unexpected("Failed to set decoder output type: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  // 检查是否有音频
  bool has_audio = false;
  wil::com_ptr<IMFMediaType> native_audio_type;
  if (SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                                           native_audio_type.put()))) {
    has_audio = true;

    // 配置音频解码为 PCM
    wil::com_ptr<IMFMediaType> audio_decode_type;
    MFCreateMediaType(audio_decode_type.put());
    audio_decode_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    audio_decode_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr,
                                audio_decode_type.get());
  }

  // 4. 创建 Video Processor
  auto vp_result = create_video_processor_for_scaling(device.get(), src_width, src_height,
                                                      target_width, target_height);
  if (!vp_result) {
    return std::unexpected("Failed to create video processor: " + vp_result.error());
  }
  auto& vp_ctx = *vp_result;

  // 5. 创建 SinkWriter（配置 D3D11 硬件编码）
  wil::com_ptr<IMFAttributes> writer_attrs;
  MFCreateAttributes(writer_attrs.put(), 2);
  writer_attrs->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, dxgi_manager.get());
  writer_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

  wil::com_ptr<IMFSinkWriter> writer;
  hr = MFCreateSinkWriterFromURL(output_path.c_str(), nullptr, writer_attrs.get(), writer.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create sink writer: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
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
  output_video_type->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);

  DWORD video_stream_index = 0;
  hr = writer->AddStream(output_video_type.get(), &video_stream_index);
  if (FAILED(hr)) {
    return std::unexpected("Failed to add video stream: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
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

  hr = writer->SetInputMediaType(video_stream_index, input_video_type.get(), nullptr);
  if (FAILED(hr)) {
    return std::unexpected("Failed to set video input type: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  // 配置音频流（如果有）
  DWORD audio_stream_index = 0;
  wil::com_ptr<IMFMediaType> actual_audio_type;
  if (has_audio) {
    reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, actual_audio_type.put());

    wil::com_ptr<IMFMediaType> output_audio_type;
    MFCreateMediaType(output_audio_type.put());
    output_audio_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    output_audio_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    output_audio_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    output_audio_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    output_audio_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    output_audio_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);

    hr = writer->AddStream(output_audio_type.get(), &audio_stream_index);
    if (FAILED(hr)) {
      Logger().warn("Failed to add audio stream, continuing without audio");
      has_audio = false;
    } else {
      hr = writer->SetInputMediaType(audio_stream_index, actual_audio_type.get(), nullptr);
      if (FAILED(hr)) {
        Logger().warn("Failed to set audio input type, continuing without audio");
        has_audio = false;
      }
    }
  }

  // 6. 开始写入
  hr = writer->BeginWriting();
  if (FAILED(hr)) {
    return std::unexpected("Failed to begin writing: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  // 7. 读取、缩放、写入循环
  bool first_video_sample = true;
  std::int64_t first_video_timestamp = 0;
  bool first_audio_sample = true;
  std::int64_t first_audio_timestamp = 0;
  std::int64_t frame_duration = 10'000'000 / config.fps;
  std::uint32_t frame_count = 0;
  std::uint32_t skipped_non_dxgi = 0;

  Logger().debug("Starting frame processing loop");

  while (true) {
    DWORD stream_index = 0;
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    wil::com_ptr<IMFSample> sample;

    hr = reader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &stream_index, &flags, &timestamp,
                            sample.put());

    if (FAILED(hr)) {
      Logger().warn("ReadSample failed: {:08X}", static_cast<std::uint32_t>(hr));
      break;
    }

    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
      Logger().debug("End of stream reached");
      break;
    }

    if ((flags & MF_SOURCE_READERF_STREAMTICK) || !sample) {
      continue;
    }

    bool is_video = (stream_index == MF_SOURCE_READER_FIRST_VIDEO_STREAM);

    if (is_video) {
      // 调整时间戳
      if (first_video_sample) {
        first_video_timestamp = timestamp;
        first_video_sample = false;
      }
      std::int64_t adjusted_ts = timestamp - first_video_timestamp;

      // 从 sample 获取纹理
      wil::com_ptr<IMFMediaBuffer> buffer;
      hr = sample->ConvertToContiguousBuffer(buffer.put());
      if (FAILED(hr)) {
        Logger().warn("Failed to get buffer from sample");
        continue;
      }

      wil::com_ptr<IMFDXGIBuffer> dxgi_buffer;
      hr = buffer->QueryInterface(IID_PPV_ARGS(dxgi_buffer.put()));
      if (FAILED(hr)) {
        skipped_non_dxgi++;
        if (skipped_non_dxgi <= 3) {
          Logger().warn("Sample is not a DXGI buffer (hr={:08X}), skipping frame",
                        static_cast<std::uint32_t>(hr));
        }
        continue;
      }

      wil::com_ptr<ID3D11Texture2D> input_texture;
      hr = dxgi_buffer->GetResource(IID_PPV_ARGS(input_texture.put()));
      if (FAILED(hr)) {
        Logger().warn("Failed to get texture from DXGI buffer");
        continue;
      }

      // 执行缩放
      hr = scale_frame(vp_ctx, input_texture.get());
      if (FAILED(hr)) {
        Logger().warn("Video Processor scaling failed: {:08X}", static_cast<std::uint32_t>(hr));
        continue;
      }

      // 创建输出 sample
      wil::com_ptr<IMFSample> output_sample;
      MFCreateSample(output_sample.put());

      wil::com_ptr<IMFMediaBuffer> output_buffer;
      wil::com_ptr<IDXGISurface> surface;
      hr = vp_ctx.output_texture->QueryInterface(IID_PPV_ARGS(surface.put()));
      if (FAILED(hr)) {
        Logger().warn("Failed to get DXGI surface from output texture");
        continue;
      }

      hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), surface.get(), 0, FALSE,
                                     output_buffer.put());
      if (FAILED(hr)) {
        Logger().warn("Failed to create DXGI buffer for output");
        continue;
      }

      // 设置 buffer 长度（NV12）
      DWORD buffer_length = target_width * target_height * 3 / 2;
      output_buffer->SetCurrentLength(buffer_length);

      output_sample->AddBuffer(output_buffer.get());
      output_sample->SetSampleTime(adjusted_ts);
      output_sample->SetSampleDuration(frame_duration);

      // 写入
      hr = writer->WriteSample(video_stream_index, output_sample.get());
      if (FAILED(hr)) {
        Logger().warn("Failed to write video sample: {:08X}", static_cast<std::uint32_t>(hr));
        continue;
      }

      frame_count++;
    } else if (has_audio) {
      // 音频直接写入
      if (first_audio_sample) {
        first_audio_timestamp = timestamp;
        first_audio_sample = false;
      }
      std::int64_t adjusted_ts = timestamp - first_audio_timestamp;
      sample->SetSampleTime(adjusted_ts);

      hr = writer->WriteSample(audio_stream_index, sample.get());
      if (FAILED(hr)) {
        Logger().warn("Failed to write audio sample");
      }
    }
  }

  // 8. Finalize
  hr = writer->Finalize();
  if (FAILED(hr)) {
    return std::unexpected("Failed to finalize output: " +
                           std::to_string(static_cast<std::uint32_t>(hr)));
  }

  if (skipped_non_dxgi > 0) {
    Logger().warn("Skipped {} frames due to non-DXGI buffers", skipped_non_dxgi);
  }

  result.scaled = true;
  Logger().info("Video scaling completed: {} frames, saved to {}", frame_count,
                output_path.string());
  return result;
}

}  // namespace Utils::Media::VideoScaler
