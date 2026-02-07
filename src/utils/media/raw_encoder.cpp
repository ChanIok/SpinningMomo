module;

#include <codecapi.h>
#include <mfidl.h>
#include <mfobjects.h>

module Utils.Media.RawEncoder;

import std;
import Utils.Logger;
import <audioclient.h>;
import <d3d11.h>;
import <mfapi.h>;
import <mferror.h>;
import <wil/com.h>;

namespace Utils::Media::RawEncoder {

// 辅助函数：从 IMFSample 提取压缩数据
auto extract_sample_data(IMFSample* sample, EncodedFrame& frame)
    -> std::expected<void, std::string> {
  if (!sample) {
    return std::unexpected("Null sample");
  }

  // 获取时间戳
  LONGLONG sample_time = 0;
  sample->GetSampleTime(&sample_time);
  frame.timestamp_100ns = sample_time;

  // 获取时长
  LONGLONG sample_duration = 0;
  sample->GetSampleDuration(&sample_duration);
  frame.duration_100ns = sample_duration;

  // 获取 buffer
  wil::com_ptr<IMFMediaBuffer> buffer;
  HRESULT hr = sample->ConvertToContiguousBuffer(buffer.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to get contiguous buffer");
  }

  // 锁定并复制数据
  BYTE* data = nullptr;
  DWORD data_length = 0;
  hr = buffer->Lock(&data, nullptr, &data_length);
  if (FAILED(hr)) {
    return std::unexpected("Failed to lock buffer");
  }

  frame.data.resize(data_length);
  std::memcpy(frame.data.data(), data, data_length);
  buffer->Unlock();

  return {};
}

// 辅助函数：检查 sample 是否为关键帧
auto is_keyframe_sample(IMFSample* sample) -> bool {
  if (!sample) return false;

  UINT32 clean_point = 0;
  if (SUCCEEDED(sample->GetUINT32(MFSampleExtension_CleanPoint, &clean_point))) {
    return clean_point != 0;
  }
  return false;
}

// 辅助函数：等待异步 MFT 事件
auto wait_events(RawEncoderContext& ctx) -> std::expected<void, std::string> {
  if (!ctx.async_events) return {};

  while (!(ctx.async_need_input || ctx.async_have_output || ctx.draining_done)) {
    wil::com_ptr<IMFMediaEvent> event;
    HRESULT hr = ctx.async_events->GetEvent(0, event.put());
    if (FAILED(hr)) {
      return std::unexpected("GetEvent failed: " + std::to_string(hr));
    }

    MediaEventType event_type;
    event->GetType(&event_type);

    switch (event_type) {
      case METransformNeedInput:
        if (!ctx.draining) ctx.async_need_input = true;
        break;
      case METransformHaveOutput:
        ctx.async_have_output = true;
        break;
      case METransformDrainComplete:
        ctx.draining_done = true;
        break;
      default:
        break;
    }
  }
  return {};
}

// 辅助函数：从 Transform 获取输出（视频用 RawEncoderContext 版本）
auto get_transform_output(RawEncoderContext& ctx, bool is_audio)
    -> std::expected<std::optional<EncodedFrame>, std::string> {
  IMFTransform* transform = is_audio ? ctx.audio_encoder.get() : ctx.video_encoder.get();
  if (!transform) {
    return std::unexpected("Transform not initialized");
  }

  MFT_OUTPUT_STREAM_INFO stream_info = {};
  HRESULT hr = transform->GetOutputStreamInfo(0, &stream_info);
  if (FAILED(hr)) {
    return std::unexpected("Failed to get output stream info");
  }

  // 准备输出 buffer
  MFT_OUTPUT_DATA_BUFFER output_buffer = {};
  output_buffer.dwStreamID = 0;

  // 检查 MFT 是否自己分配 sample
  bool mft_provides_samples = (stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) != 0;

  wil::com_ptr<IMFSample> output_sample;
  if (!mft_provides_samples) {
    // 我们需要提供 sample
    hr = MFCreateSample(output_sample.put());
    if (FAILED(hr)) {
      return std::unexpected("Failed to create output sample");
    }

    wil::com_ptr<IMFMediaBuffer> output_media_buffer;
    DWORD buffer_size = stream_info.cbSize > 0 ? stream_info.cbSize : (1024 * 1024);  // 默认 1MB
    hr = MFCreateMemoryBuffer(buffer_size, output_media_buffer.put());
    if (FAILED(hr)) {
      return std::unexpected("Failed to create output buffer");
    }

    output_sample->AddBuffer(output_media_buffer.get());
    output_buffer.pSample = output_sample.get();
  }

  DWORD status = 0;
  hr = transform->ProcessOutput(0, 1, &output_buffer, &status);

  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    // 没有输出可用，这是正常的（同步模式）
    return std::nullopt;
  }

  if (FAILED(hr)) {
    return std::unexpected("ProcessOutput failed: " + std::to_string(hr));
  }

  // 异步模式：已消费一个输出事件
  if (ctx.is_async && !is_audio) {
    ctx.async_have_output = false;
  }

  // 如果 MFT 提供了 sample
  IMFSample* result_sample = mft_provides_samples ? output_buffer.pSample : output_sample.get();
  if (!result_sample) {
    return std::nullopt;
  }

  EncodedFrame frame;
  frame.is_audio = is_audio;
  frame.is_keyframe = !is_audio && is_keyframe_sample(result_sample);

  auto extract_result = extract_sample_data(result_sample, frame);
  if (!extract_result) {
    // 释放 MFT 分配的 sample
    if (mft_provides_samples && output_buffer.pSample) {
      output_buffer.pSample->Release();
    }
    return std::unexpected(extract_result.error());
  }

  // 释放 MFT 分配的 sample
  if (mft_provides_samples && output_buffer.pSample) {
    output_buffer.pSample->Release();
  }

  return frame;
}

// 辅助函数：创建 D3D11 Video Processor 用于 BGRA→NV12 转换
auto create_video_processor(RawEncoderContext& ctx, ID3D11Device* device, std::uint32_t width,
                            std::uint32_t height) -> std::expected<void, std::string> {
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

  // 创建 Video Processor Enumerator
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
  content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  content_desc.InputWidth = width;
  content_desc.InputHeight = height;
  content_desc.OutputWidth = width;
  content_desc.OutputHeight = height;
  content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  hr = ctx.video_device->CreateVideoProcessorEnumerator(&content_desc, ctx.vp_enum.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create video processor enumerator");
  }

  // 创建 Video Processor
  hr = ctx.video_device->CreateVideoProcessor(ctx.vp_enum.get(), 0, ctx.video_processor.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create video processor");
  }

  // 创建 NV12 中间纹理（编码器从这里读取）
  D3D11_TEXTURE2D_DESC nv12_desc = {};
  nv12_desc.Width = width;
  nv12_desc.Height = height;
  nv12_desc.MipLevels = 1;
  nv12_desc.ArraySize = 1;
  nv12_desc.Format = DXGI_FORMAT_NV12;
  nv12_desc.SampleDesc.Count = 1;
  nv12_desc.Usage = D3D11_USAGE_DEFAULT;
  nv12_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

  hr = device->CreateTexture2D(&nv12_desc, nullptr, ctx.nv12_texture.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create NV12 texture");
  }

  ctx.needs_nv12_conversion = true;
  Logger().info("D3D11 Video Processor created for BGRA->NV12 conversion");
  return {};
}

// 辅助函数：执行 BGRA→NV12 转换
auto convert_bgra_to_nv12(RawEncoderContext& ctx, ID3D11Texture2D* bgra_texture) -> HRESULT {
  // 创建输入视图 (BGRA)
  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {};
  input_view_desc.FourCC = 0;
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.MipSlice = 0;

  wil::com_ptr<ID3D11VideoProcessorInputView> input_view;
  HRESULT hr = ctx.video_device->CreateVideoProcessorInputView(bgra_texture, ctx.vp_enum.get(),
                                                               &input_view_desc, input_view.put());
  if (FAILED(hr)) return hr;

  // 创建输出视图 (NV12)
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {};
  output_view_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  output_view_desc.Texture2D.MipSlice = 0;

  wil::com_ptr<ID3D11VideoProcessorOutputView> output_view;
  hr = ctx.video_device->CreateVideoProcessorOutputView(ctx.nv12_texture.get(), ctx.vp_enum.get(),
                                                        &output_view_desc, output_view.put());
  if (FAILED(hr)) return hr;

  // 执行转换
  D3D11_VIDEO_PROCESSOR_STREAM stream = {};
  stream.Enable = TRUE;
  stream.OutputIndex = 0;
  stream.InputFrameOrField = 0;
  stream.pInputSurface = input_view.get();

  return ctx.video_context->VideoProcessorBlt(ctx.video_processor.get(), output_view.get(), 0, 1,
                                              &stream);
}

// 创建视频编码器 Transform
auto create_video_encoder(RawEncoderContext& ctx, const RawEncoderConfig& config,
                          ID3D11Device* device) -> std::expected<void, std::string> {
  // 1. 枚举 H.264 编码器（包含异步 MFT，硬件编码器通常是异步的）
  MFT_REGISTER_TYPE_INFO output_type_info = {};
  output_type_info.guidMajorType = MFMediaType_Video;
  output_type_info.guidSubtype = MFVideoFormat_H264;

  UINT32 flags = MFT_ENUM_FLAG_SORTANDFILTER;
  if (config.use_hardware && device) {
    flags |= MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_ASYNCMFT;
  } else {
    flags |= MFT_ENUM_FLAG_SYNCMFT;
  }

  IMFActivate** activates = nullptr;
  UINT32 count = 0;
  HRESULT hr =
      MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, nullptr, &output_type_info, &activates, &count);

  // 如果硬件编码器找不到，回退到软件
  if ((FAILED(hr) || count == 0) && config.use_hardware) {
    Logger().warn("No hardware H.264 encoder found, falling back to software");
    flags = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER;
    hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, nullptr, &output_type_info, &activates,
                   &count);
  }

  if (FAILED(hr) || count == 0) {
    return std::unexpected("No H.264 encoder found");
  }

  // 激活第一个编码器
  hr = activates[0]->ActivateObject(IID_PPV_ARGS(ctx.video_encoder.put()));

  // 释放 activates
  for (UINT32 i = 0; i < count; i++) {
    activates[i]->Release();
  }
  CoTaskMemFree(activates);

  if (FAILED(hr)) {
    return std::unexpected("Failed to activate encoder");
  }

  // 2. 解锁异步 MFT 并获取事件生成器
  wil::com_ptr<IMFAttributes> encoder_attrs;
  hr = ctx.video_encoder->GetAttributes(encoder_attrs.put());
  if (SUCCEEDED(hr)) {
    UINT32 is_async = 0;
    hr = encoder_attrs->GetUINT32(MF_TRANSFORM_ASYNC, &is_async);
    if (SUCCEEDED(hr) && is_async) {
      encoder_attrs->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
      Logger().info("Async MFT detected and unlocked");

      // 获取事件生成器（异步 MFT 必须通过事件驱动 ProcessInput/ProcessOutput）
      hr = ctx.video_encoder->QueryInterface(IID_PPV_ARGS(ctx.async_events.put()));
      if (SUCCEEDED(hr)) {
        ctx.is_async = true;
        Logger().info("Async MFT event generator acquired");
      } else {
        Logger().warn("Failed to get IMFMediaEventGenerator, falling back to sync mode");
      }
    }
  }

  // 3. 如果是硬件编码，设置 D3D Manager
  if (config.use_hardware && device) {
    hr = MFCreateDXGIDeviceManager(&ctx.dxgi_reset_token, ctx.dxgi_manager.put());
    if (FAILED(hr)) {
      return std::unexpected("Failed to create DXGI manager");
    }

    hr = ctx.dxgi_manager->ResetDevice(device, ctx.dxgi_reset_token);
    if (FAILED(hr)) {
      return std::unexpected("Failed to reset DXGI device");
    }

    hr = ctx.video_encoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                           reinterpret_cast<ULONG_PTR>(ctx.dxgi_manager.get()));
    if (FAILED(hr)) {
      Logger().warn("Failed to set D3D manager on encoder, falling back to CPU");
      ctx.gpu_encoding = false;
    } else {
      ctx.gpu_encoding = true;
    }
  }

  // 4. 设置输出媒体类型（H.264）
  wil::com_ptr<IMFMediaType> output_type;
  hr = MFCreateMediaType(output_type.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create output media type");
  }

  output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  output_type->SetUINT32(MF_MT_AVG_BITRATE, config.bitrate);
  output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  MFSetAttributeSize(output_type.get(), MF_MT_FRAME_SIZE, config.width, config.height);
  MFSetAttributeRatio(output_type.get(), MF_MT_FRAME_RATE, config.fps, 1);
  MFSetAttributeRatio(output_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  output_type->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
  output_type->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, config.fps * config.keyframe_interval);

  hr = ctx.video_encoder->SetOutputType(0, output_type.get(), 0);
  if (FAILED(hr)) {
    return std::unexpected("Failed to set encoder output type: " + std::to_string(hr));
  }

  // 保存输出类型（包含 SPS/PPS）
  hr = ctx.video_encoder->GetOutputCurrentType(0, ctx.video_output_type.put());
  if (FAILED(hr)) {
    ctx.video_output_type = output_type;
  }

  // 5. 枚举编码器支持的输入类型并选择最佳格式
  bool input_type_set = false;
  GUID chosen_subtype = {};

  // 优先尝试 ARGB32（无需颜色转换）
  {
    wil::com_ptr<IMFMediaType> argb_type;
    MFCreateMediaType(argb_type.put());
    argb_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    argb_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
    argb_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(argb_type.get(), MF_MT_FRAME_SIZE, config.width, config.height);
    MFSetAttributeRatio(argb_type.get(), MF_MT_FRAME_RATE, config.fps, 1);
    MFSetAttributeRatio(argb_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    if (SUCCEEDED(ctx.video_encoder->SetInputType(0, argb_type.get(), 0))) {
      input_type_set = true;
      chosen_subtype = MFVideoFormat_ARGB32;
      Logger().info("RawEncoder using ARGB32 input (no conversion needed)");
    }
  }

  // 如果 ARGB32 不行，枚举编码器实际支持的类型
  if (!input_type_set) {
    for (DWORD i = 0;; i++) {
      wil::com_ptr<IMFMediaType> available_type;
      hr = ctx.video_encoder->GetInputAvailableType(0, i, available_type.put());
      if (FAILED(hr)) break;

      // 在枚举到的类型上设置我们的参数
      MFSetAttributeSize(available_type.get(), MF_MT_FRAME_SIZE, config.width, config.height);
      MFSetAttributeRatio(available_type.get(), MF_MT_FRAME_RATE, config.fps, 1);
      MFSetAttributeRatio(available_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
      available_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

      hr = ctx.video_encoder->SetInputType(0, available_type.get(), 0);
      if (SUCCEEDED(hr)) {
        available_type->GetGUID(MF_MT_SUBTYPE, &chosen_subtype);
        input_type_set = true;

        // 记录选中的格式
        if (chosen_subtype == MFVideoFormat_NV12) {
          Logger().info("RawEncoder using NV12 input (BGRA->NV12 conversion needed)");
        } else {
          Logger().info("RawEncoder using enumerated input type index {}", i);
        }
        break;
      }
    }
  }

  if (!input_type_set) {
    return std::unexpected("Failed to set any encoder input type");
  }

  // 6. 通知开始流
  hr = ctx.video_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  if (FAILED(hr)) {
    Logger().warn("Failed to notify begin streaming");
  }

  hr = ctx.video_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  if (FAILED(hr)) {
    Logger().warn("Failed to notify start of stream");
  }

  // 7. 缓存信息
  ctx.frame_width = config.width;
  ctx.frame_height = config.height;
  ctx.fps = config.fps;

  // 8. 创建输入纹理和颜色转换器
  if (ctx.gpu_encoding && device) {
    bool need_nv12 = (chosen_subtype == MFVideoFormat_NV12);

    if (need_nv12) {
      // 需要 BGRA→NV12 转换：创建 BGRA 输入纹理 + Video Processor
      D3D11_TEXTURE2D_DESC bgra_desc = {};
      bgra_desc.Width = config.width;
      bgra_desc.Height = config.height;
      bgra_desc.MipLevels = 1;
      bgra_desc.ArraySize = 1;
      bgra_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      bgra_desc.SampleDesc.Count = 1;
      bgra_desc.Usage = D3D11_USAGE_DEFAULT;
      bgra_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      hr = device->CreateTexture2D(&bgra_desc, nullptr, ctx.input_texture.put());
      if (FAILED(hr)) {
        return std::unexpected("Failed to create BGRA input texture");
      }

      auto vp_result = create_video_processor(ctx, device, config.width, config.height);
      if (!vp_result) {
        return std::unexpected("Failed to create video processor: " + vp_result.error());
      }
    } else {
      // 不需要转换：直接创建 BGRA 纹理给编码器
      D3D11_TEXTURE2D_DESC desc = {};
      desc.Width = config.width;
      desc.Height = config.height;
      desc.MipLevels = 1;
      desc.ArraySize = 1;
      desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      hr = device->CreateTexture2D(&desc, nullptr, ctx.input_texture.put());
      if (FAILED(hr)) {
        return std::unexpected("Failed to create input texture");
      }
    }
  }

  Logger().info("RawEncoder video encoder created: {}x{} @ {}fps, {} bps, GPU={}, NV12={}",
                config.width, config.height, config.fps, config.bitrate, ctx.gpu_encoding,
                ctx.needs_nv12_conversion);

  return {};
}

// 创建音频编码器 Transform
auto create_audio_encoder(RawEncoderContext& ctx, WAVEFORMATEX* wave_format)
    -> std::expected<void, std::string> {
  if (!wave_format) {
    return {};
  }

  // 枚举 AAC 编码器
  MFT_REGISTER_TYPE_INFO output_type_info = {};
  output_type_info.guidMajorType = MFMediaType_Audio;
  output_type_info.guidSubtype = MFAudioFormat_AAC;

  IMFActivate** activates = nullptr;
  UINT32 count = 0;
  HRESULT hr = MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SYNCMFT, nullptr,
                         &output_type_info, &activates, &count);

  if (FAILED(hr) || count == 0) {
    Logger().warn("No AAC encoder found, audio will be disabled");
    return {};
  }

  hr = activates[0]->ActivateObject(IID_PPV_ARGS(ctx.audio_encoder.put()));

  for (UINT32 i = 0; i < count; i++) {
    activates[i]->Release();
  }
  CoTaskMemFree(activates);

  if (FAILED(hr)) {
    Logger().warn("Failed to activate AAC encoder");
    return {};
  }

  // 设置输出类型（AAC）
  wil::com_ptr<IMFMediaType> output_type;
  MFCreateMediaType(output_type.put());
  output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  output_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
  output_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wave_format->nSamplesPerSec);
  output_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wave_format->nChannels);
  output_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
  output_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);  // ~128kbps

  hr = ctx.audio_encoder->SetOutputType(0, output_type.get(), 0);
  if (FAILED(hr)) {
    Logger().warn("Failed to set audio output type");
    ctx.audio_encoder = nullptr;
    return {};
  }

  // 获取实际的输出类型
  ctx.audio_encoder->GetOutputCurrentType(0, ctx.audio_output_type.put());
  if (!ctx.audio_output_type) {
    ctx.audio_output_type = output_type;
  }

  // 设置输入类型（PCM）
  wil::com_ptr<IMFMediaType> input_type;
  MFCreateMediaType(input_type.put());
  MFInitMediaTypeFromWaveFormatEx(input_type.get(), wave_format,
                                  sizeof(WAVEFORMATEX) + wave_format->cbSize);

  hr = ctx.audio_encoder->SetInputType(0, input_type.get(), 0);
  if (FAILED(hr)) {
    Logger().warn("Failed to set audio input type");
    ctx.audio_encoder = nullptr;
    return {};
  }

  // 通知开始流
  ctx.audio_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  ctx.audio_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

  ctx.has_audio = true;
  Logger().info("RawEncoder audio encoder created");

  return {};
}

auto create_encoder(const RawEncoderConfig& config, ID3D11Device* device, WAVEFORMATEX* wave_format)
    -> std::expected<RawEncoderContext, std::string> {
  RawEncoderContext ctx;

  // 创建视频编码器
  auto video_result = create_video_encoder(ctx, config, device);
  if (!video_result) {
    return std::unexpected(video_result.error());
  }

  // 创建音频编码器
  auto audio_result = create_audio_encoder(ctx, wave_format);
  if (!audio_result) {
    Logger().warn("Audio encoder creation failed: {}", audio_result.error());
  }

  return ctx;
}

auto encode_video_frame(RawEncoderContext& ctx, ID3D11DeviceContext* context,
                        ID3D11Texture2D* texture, std::int64_t timestamp_100ns)
    -> std::expected<std::vector<EncodedFrame>, std::string> {
  if (!ctx.video_encoder) {
    return std::unexpected("Video encoder not initialized");
  }

  std::vector<EncodedFrame> outputs;

  // 1. 创建输入 sample
  wil::com_ptr<IMFSample> input_sample;
  HRESULT hr = MFCreateSample(input_sample.put());
  if (FAILED(hr)) {
    return std::unexpected("Failed to create input sample");
  }

  wil::com_ptr<IMFMediaBuffer> input_buffer;

  if (ctx.gpu_encoding && ctx.input_texture && context) {
    // GPU 路径：复制 BGRA 纹理到输入纹理
    context->CopyResource(ctx.input_texture.get(), texture);

    // 确定编码器实际接收的纹理（可能需要 NV12 转换）
    ID3D11Texture2D* encoder_texture = ctx.input_texture.get();
    DWORD buffer_length = ctx.frame_width * ctx.frame_height * 4;  // BGRA

    if (ctx.needs_nv12_conversion && ctx.nv12_texture) {
      // BGRA→NV12 转换（硬件加速）
      hr = convert_bgra_to_nv12(ctx, ctx.input_texture.get());
      if (FAILED(hr)) {
        return std::unexpected("BGRA->NV12 conversion failed: " + std::to_string(hr));
      }
      encoder_texture = ctx.nv12_texture.get();
      buffer_length = ctx.frame_width * ctx.frame_height * 3 / 2;  // NV12
    }

    wil::com_ptr<IDXGISurface> surface;
    hr = encoder_texture->QueryInterface(IID_PPV_ARGS(surface.put()));
    if (FAILED(hr)) {
      return std::unexpected("Failed to get DXGI surface");
    }

    hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), surface.get(), 0, FALSE,
                                   input_buffer.put());
    if (FAILED(hr)) {
      return std::unexpected("Failed to create DXGI buffer");
    }

    input_buffer->SetCurrentLength(buffer_length);
  } else {
    // CPU 路径：从纹理复制到内存
    if (!ctx.staging_texture) {
      D3D11_TEXTURE2D_DESC desc;
      texture->GetDesc(&desc);
      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.MiscFlags = 0;

      wil::com_ptr<ID3D11Device> device;
      texture->GetDevice(device.put());
      hr = device->CreateTexture2D(&desc, nullptr, ctx.staging_texture.put());
      if (FAILED(hr)) {
        return std::unexpected("Failed to create staging texture");
      }
    }

    context->CopyResource(ctx.staging_texture.get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(ctx.staging_texture.get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
      return std::unexpected("Failed to map staging texture");
    }

    DWORD buffer_size = ctx.frame_width * ctx.frame_height * 4;
    hr = MFCreateMemoryBuffer(buffer_size, input_buffer.put());
    if (FAILED(hr)) {
      context->Unmap(ctx.staging_texture.get(), 0);
      return std::unexpected("Failed to create memory buffer");
    }

    BYTE* dest = nullptr;
    hr = input_buffer->Lock(&dest, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      const BYTE* src = static_cast<const BYTE*>(mapped.pData);
      UINT row_pitch = ctx.frame_width * 4;

      if (row_pitch == mapped.RowPitch) {
        std::memcpy(dest, src, buffer_size);
      } else {
        for (UINT y = 0; y < ctx.frame_height; y++) {
          std::memcpy(dest + y * row_pitch, src + y * mapped.RowPitch, row_pitch);
        }
      }
      input_buffer->Unlock();
      input_buffer->SetCurrentLength(buffer_size);
    }

    context->Unmap(ctx.staging_texture.get(), 0);
  }

  input_sample->AddBuffer(input_buffer.get());
  input_sample->SetSampleTime(timestamp_100ns);
  input_sample->SetSampleDuration(10'000'000 / ctx.fps);

  // 2. 异步模式：循环等 need_input，途中消费 have_output
  if (ctx.is_async) {
    while (true) {
      auto wait_result = wait_events(ctx);
      if (!wait_result) return std::unexpected(wait_result.error());

      if (ctx.async_have_output) {
        auto output = get_transform_output(ctx, false);
        if (!output) return std::unexpected(output.error());
        if (output->has_value()) {
          outputs.push_back(std::move(output->value()));
        }
        // 继续等 need_input
        continue;
      }

      if (ctx.async_need_input) {
        break;
      }
    }
  }

  // 3. 送入编码器
  hr = ctx.video_encoder->ProcessInput(0, input_sample.get(), 0);
  if (FAILED(hr)) {
    return std::unexpected("ProcessInput failed: " + std::to_string(hr));
  }
  ctx.async_need_input = false;

  // 4. 同步模式：尝试获取输出
  if (!ctx.is_async) {
    auto output = get_transform_output(ctx, false);
    if (!output) return std::unexpected(output.error());
    if (output->has_value()) {
      outputs.push_back(std::move(output->value()));
    }
  }

  return outputs;
}

auto encode_audio_frame(RawEncoderContext& ctx, const BYTE* pcm_data, std::uint32_t pcm_size,
                        std::int64_t timestamp_100ns)
    -> std::expected<std::optional<EncodedFrame>, std::string> {
  if (!ctx.audio_encoder || !ctx.has_audio) {
    return std::nullopt;
  }

  // 创建输入 sample
  wil::com_ptr<IMFSample> input_sample;
  MFCreateSample(input_sample.put());

  wil::com_ptr<IMFMediaBuffer> input_buffer;
  MFCreateMemoryBuffer(pcm_size, input_buffer.put());

  BYTE* dest = nullptr;
  input_buffer->Lock(&dest, nullptr, nullptr);
  std::memcpy(dest, pcm_data, pcm_size);
  input_buffer->Unlock();
  input_buffer->SetCurrentLength(pcm_size);

  input_sample->AddBuffer(input_buffer.get());
  input_sample->SetSampleTime(timestamp_100ns);

  // 送入编码器
  HRESULT hr = ctx.audio_encoder->ProcessInput(0, input_sample.get(), 0);
  if (FAILED(hr) && hr != MF_E_NOTACCEPTING) {
    return std::unexpected("Audio ProcessInput failed");
  }

  // 尝试获取输出
  return get_transform_output(ctx, true);
}

auto flush_encoder(RawEncoderContext& ctx)
    -> std::expected<std::vector<EncodedFrame>, std::string> {
  std::vector<EncodedFrame> frames;

  // Drain 视频编码器
  if (ctx.video_encoder) {
    ctx.video_encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);

    if (ctx.is_async) {
      // 异步模式：通过事件等待 drain 完成
      ctx.draining = true;

      while (!ctx.draining_done) {
        auto wait_result = wait_events(ctx);
        if (!wait_result) break;

        if (ctx.async_have_output) {
          auto output = get_transform_output(ctx, false);
          if (output && output->has_value()) {
            frames.push_back(std::move(output->value()));
          }
        }
      }
    } else {
      // 同步模式：循环取输出直到没有更多
      while (true) {
        auto output = get_transform_output(ctx, false);
        if (!output) break;
        if (output->has_value()) {
          frames.push_back(std::move(output->value()));
        } else {
          break;
        }
      }
    }
  }

  // Drain 音频编码器（始终是同步 MFT）
  if (ctx.audio_encoder && ctx.has_audio) {
    ctx.audio_encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);

    while (true) {
      auto output = get_transform_output(ctx, true);
      if (!output) break;
      if (output->has_value()) {
        frames.push_back(std::move(output->value()));
      } else {
        break;
      }
    }
  }

  return frames;
}

auto get_video_codec_private_data(const RawEncoderContext& ctx) -> std::vector<std::uint8_t> {
  std::vector<std::uint8_t> data;

  if (!ctx.video_output_type) {
    return data;
  }

  UINT32 blob_size = 0;
  HRESULT hr = ctx.video_output_type->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &blob_size);
  if (FAILED(hr) || blob_size == 0) {
    return data;
  }

  data.resize(blob_size);
  hr = ctx.video_output_type->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER, data.data(), blob_size, nullptr);
  if (FAILED(hr)) {
    data.clear();
  }

  return data;
}

auto get_video_output_type(const RawEncoderContext& ctx) -> IMFMediaType* {
  return ctx.video_output_type.get();
}

auto get_audio_output_type(const RawEncoderContext& ctx) -> IMFMediaType* {
  return ctx.audio_output_type.get();
}

auto finalize(RawEncoderContext& ctx) -> void {
  if (ctx.video_encoder) {
    ctx.video_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
    ctx.video_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
  }

  if (ctx.audio_encoder) {
    ctx.audio_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
    ctx.audio_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
  }

  ctx.async_events = nullptr;
  ctx.is_async = false;
  ctx.async_need_input = false;
  ctx.async_have_output = false;
  ctx.draining = false;
  ctx.draining_done = false;
  ctx.video_encoder = nullptr;
  ctx.audio_encoder = nullptr;
  ctx.dxgi_manager = nullptr;
  ctx.video_output_type = nullptr;
  ctx.audio_output_type = nullptr;
  ctx.input_texture = nullptr;
  ctx.staging_texture = nullptr;
  ctx.nv12_texture = nullptr;
  ctx.video_processor = nullptr;
  ctx.vp_enum = nullptr;
  ctx.video_context = nullptr;
  ctx.video_device = nullptr;
  ctx.needs_nv12_conversion = false;
}

}  // namespace Utils::Media::RawEncoder
