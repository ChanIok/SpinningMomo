module;

#include <mfidl.h>
#include <mfreadwrite.h>

module Utils.Media.Encoder;

import std;
import Utils.Logger;
import <d3d11.h>;
import <mfapi.h>;
import <mferror.h>;
import <wil/com.h>;
import <windows.h>;
import <codecapi.h>;
import <strmif.h>;

namespace Utils::Media::Encoder {

// 辅助函数：创建输出媒体类型
auto create_output_media_type(uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate,
                              uint32_t keyframe_interval, Types::VideoCodec codec)
    -> wil::com_ptr<IMFMediaType> {
  wil::com_ptr<IMFMediaType> media_type;
  if (FAILED(MFCreateMediaType(media_type.put()))) return nullptr;
  if (FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))) return nullptr;

  // 根据 codec 选择编码格式
  GUID subtype = (codec == Types::VideoCodec::H265) ? MFVideoFormat_HEVC : MFVideoFormat_H264;
  if (FAILED(media_type->SetGUID(MF_MT_SUBTYPE, subtype))) return nullptr;
  if (FAILED(media_type->SetUINT32(MF_MT_AVG_BITRATE, bitrate))) return nullptr;
  if (FAILED(media_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)))
    return nullptr;
  if (FAILED(MFSetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, width, height))) return nullptr;
  if (FAILED(MFSetAttributeRatio(media_type.get(), MF_MT_FRAME_RATE, fps, 1))) return nullptr;
  if (FAILED(MFSetAttributeRatio(media_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1))) return nullptr;

  // 颜色元数据 (BT.709 标准，与 NVIDIA APP / OBS 一致)
  media_type->SetUINT32(MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
  media_type->SetUINT32(MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
  media_type->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
  media_type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235);  // TV / Limited Range

  // 编码 Profile (H.264 High / H.265 Main，与 NVIDIA APP / OBS 一致)
  if (codec == Types::VideoCodec::H265) {
    media_type->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH265VProfile_Main_420_8);
  } else {
    media_type->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
  }

  // 关键帧间隔
  media_type->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, fps * keyframe_interval);

  return media_type;
}

// 辅助函数：创建输入媒体类型
auto create_input_media_type(uint32_t width, uint32_t height, uint32_t fps, bool set_stride)
    -> wil::com_ptr<IMFMediaType> {
  wil::com_ptr<IMFMediaType> media_type;
  if (FAILED(MFCreateMediaType(media_type.put()))) return nullptr;
  if (FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))) return nullptr;
  if (FAILED(media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32))) return nullptr;
  if (FAILED(media_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)))
    return nullptr;
  if (FAILED(MFSetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, width, height))) return nullptr;

  if (set_stride) {
    const INT32 stride = -static_cast<INT32>(width * 4);
    if (FAILED(media_type->SetUINT32(MF_MT_DEFAULT_STRIDE, static_cast<UINT32>(stride))))
      return nullptr;
  }

  if (FAILED(MFSetAttributeRatio(media_type.get(), MF_MT_FRAME_RATE, fps, 1))) return nullptr;
  if (FAILED(MFSetAttributeRatio(media_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1))) return nullptr;

  // 输入颜色信息：屏幕捕获是 Full Range RGB，告知编码器以正确执行 RGB→YUV 转换
  media_type->SetUINT32(MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
  media_type->SetUINT32(MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
  media_type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_0_255);  // Full Range (屏幕捕获)

  return media_type;
}

// 辅助函数：添加音频流
auto add_audio_stream(State::EncoderContext& encoder, WAVEFORMATEX* wave_format,
                      uint32_t audio_bitrate) -> std::expected<void, std::string> {
  if (!encoder.sink_writer) {
    return std::unexpected("Sink writer not initialized");
  }

  if (!wave_format) {
    return std::unexpected("Invalid wave format");
  }

  // 1. 创建 AAC 输出媒体类型
  wil::com_ptr<IMFMediaType> audio_out;
  if (FAILED(MFCreateMediaType(audio_out.put()))) {
    return std::unexpected("Failed to create audio output media type");
  }

  if (FAILED(audio_out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))) {
    return std::unexpected("Failed to set audio major type");
  }

  if (FAILED(audio_out->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC))) {
    return std::unexpected("Failed to set AAC subtype");
  }

  if (FAILED(audio_out->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wave_format->nSamplesPerSec))) {
    return std::unexpected("Failed to set audio sample rate");
  }

  if (FAILED(audio_out->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wave_format->nChannels))) {
    return std::unexpected("Failed to set audio channel count");
  }

  if (FAILED(audio_out->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16))) {
    return std::unexpected("Failed to set audio bits per sample");
  }

  // 设置 AAC 码率（使用配置的码率）
  if (FAILED(audio_out->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, audio_bitrate / 8))) {
    return std::unexpected("Failed to set audio bitrate");
  }

  // 添加音频流
  if (FAILED(encoder.sink_writer->AddStream(audio_out.get(), &encoder.audio_stream_index))) {
    return std::unexpected("Failed to add audio stream");
  }

  // 2. 创建 PCM 输入媒体类型（直接使用 WASAPI 返回的 16-bit PCM 格式）
  wil::com_ptr<IMFMediaType> audio_in;
  if (FAILED(MFCreateMediaType(audio_in.put()))) {
    return std::unexpected("Failed to create audio input media type");
  }

  UINT32 wave_format_size = sizeof(WAVEFORMATEX) + wave_format->cbSize;
  if (FAILED(MFInitMediaTypeFromWaveFormatEx(audio_in.get(), wave_format, wave_format_size))) {
    return std::unexpected("Failed to initialize audio input from wave format");
  }

  // 设置输入媒体类型
  if (FAILED(encoder.sink_writer->SetInputMediaType(encoder.audio_stream_index, audio_in.get(),
                                                    nullptr))) {
    return std::unexpected("Failed to set audio input media type");
  }

  encoder.has_audio = true;
  Logger().info("Audio stream added: {} Hz, {} channels, {} kbps", wave_format->nSamplesPerSec,
                wave_format->nChannels, audio_bitrate / 1000);

  return {};
}

// 尝试创建 GPU 编码器
auto try_create_gpu_encoder(State::EncoderContext& ctx, const Types::EncoderConfig& config,
                            ID3D11Device* device, WAVEFORMATEX* wave_format)
    -> std::expected<void, std::string> {
  // 1. 创建 DXGI Device Manager
  if (FAILED(MFCreateDXGIDeviceManager(&ctx.reset_token, ctx.dxgi_manager.put()))) {
    return std::unexpected("Failed to create DXGI Device Manager");
  }

  // 2. 将 D3D11 设备关联到 DXGI Manager
  if (FAILED(ctx.dxgi_manager->ResetDevice(device, ctx.reset_token))) {
    return std::unexpected("Failed to reset DXGI device");
  }

  // 3. 确保目录存在
  std::filesystem::create_directories(config.output_path.parent_path());

  // 4. 创建 Byte Stream
  wil::com_ptr<IMFByteStream> byte_stream;
  if (FAILED(MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE,
                          config.output_path.c_str(), byte_stream.put()))) {
    return std::unexpected("Failed to create byte stream");
  }

  // 5. 创建 Media Sink
  wil::com_ptr<IMFMediaSink> media_sink;
  if (FAILED(MFCreateMPEG4MediaSink(byte_stream.get(), nullptr, nullptr, media_sink.put()))) {
    return std::unexpected("Failed to create MPEG4 media sink");
  }

  // 6. 创建 Sink Writer 属性并预设编码器参数
  wil::com_ptr<IMFAttributes> attributes;
  if (FAILED(MFCreateAttributes(attributes.put(), 8))) {
    return std::unexpected("Failed to create MF attributes");
  }

  // 启用硬件加速
  if (FAILED(attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE))) {
    return std::unexpected("Failed to enable hardware transforms");
  }

  // 设置 DXGI Manager
  if (FAILED(attributes->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, ctx.dxgi_manager.get()))) {
    return std::unexpected("Failed to set D3D manager");
  }

  // 在创建 Sink Writer 前预设 Rate Control Mode
  UINT32 rate_control_mode = eAVEncCommonRateControlMode_CBR;  // 默认 CBR

  if (config.rate_control == Types::RateControlMode::VBR) {
    rate_control_mode = eAVEncCommonRateControlMode_Quality;
  } else if (config.rate_control == Types::RateControlMode::ManualQP) {
    rate_control_mode = eAVEncCommonRateControlMode_Quality;  // QP 模式也使用 Quality
  }

  if (FAILED(attributes->SetUINT32(CODECAPI_AVEncCommonRateControlMode, rate_control_mode))) {
    Logger().warn("Failed to set rate control mode attribute");
  }

  // 预设 Quality (VBR 模式) 或 QP (ManualQP 模式)
  if (config.rate_control == Types::RateControlMode::VBR) {
    if (FAILED(attributes->SetUINT32(CODECAPI_AVEncCommonQuality, config.quality))) {
      Logger().warn("Failed to set quality attribute");
    }
    Logger().info("GPU encoder configured for VBR mode with quality: {}", config.quality);
  } else if (config.rate_control == Types::RateControlMode::ManualQP) {
    // 尝试设置 QP 参数（可能不被所有编码器支持）
    if (FAILED(attributes->SetUINT32(CODECAPI_AVEncVideoEncodeQP, config.qp))) {
      Logger().warn("Failed to set QP attribute - encoder may not support Manual QP mode");
    }
    Logger().info("GPU encoder configured for Manual QP mode with QP: {}", config.qp);
  } else {
    Logger().info("GPU encoder configured for CBR mode");
  }

  // 7. 用预设属性创建 Sink Writer
  if (FAILED(MFCreateSinkWriterFromMediaSink(media_sink.get(), attributes.get(),
                                             ctx.sink_writer.put()))) {
    return std::unexpected("Failed to create Sink Writer from media sink");
  }

  // 8. 创建输出媒体类型
  auto media_type_out =
      create_output_media_type(config.width, config.height, config.fps, config.bitrate,
                               config.keyframe_interval, config.codec);
  if (!media_type_out) {
    return std::unexpected("Failed to create output media type");
  }

  if (FAILED(ctx.sink_writer->AddStream(media_type_out.get(), &ctx.video_stream_index))) {
    return std::unexpected("Failed to add video stream");
  }

  // 9. 创建 GPU 输入媒体类型
  auto media_type_in = create_input_media_type(config.width, config.height, config.fps, false);
  if (!media_type_in) {
    return std::unexpected("Failed to create GPU input media type");
  }

  if (FAILED(ctx.sink_writer->SetInputMediaType(ctx.video_stream_index, media_type_in.get(),
                                                nullptr))) {
    return std::unexpected("Failed to set GPU input media type");
  }

  // 10. 创建共享纹理 (编码器专用)
  D3D11_TEXTURE2D_DESC tex_desc = {};
  tex_desc.Width = config.width;
  tex_desc.Height = config.height;
  tex_desc.MipLevels = 1;
  tex_desc.ArraySize = 1;
  tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  tex_desc.SampleDesc.Count = 1;
  tex_desc.Usage = D3D11_USAGE_DEFAULT;
  tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;  // 允许作为渲染目标
  tex_desc.MiscFlags = 0;

  if (FAILED(device->CreateTexture2D(&tex_desc, nullptr, ctx.shared_texture.put()))) {
    return std::unexpected("Failed to create shared texture for GPU encoding");
  }

  // 11. 如果有音频格式，添加音频流（必须在 BeginWriting 之前）
  if (wave_format) {
    auto audio_result = add_audio_stream(ctx, wave_format, config.audio_bitrate);
    if (!audio_result) {
      Logger().warn("Failed to add audio stream to GPU encoder: {}", audio_result.error());
    }
  }

  // 12. 开始写入
  if (FAILED(ctx.sink_writer->BeginWriting())) {
    return std::unexpected("Failed to begin writing with GPU encoder");
  }

  // 缓存尺寸信息
  ctx.frame_width = config.width;
  ctx.frame_height = config.height;
  ctx.buffer_size = config.width * config.height * 4;
  ctx.gpu_encoding = true;
  return {};
}

// 创建 CPU 编码器 (fallback)
auto create_cpu_encoder(State::EncoderContext& ctx, const Types::EncoderConfig& config,
                        WAVEFORMATEX* wave_format) -> std::expected<void, std::string> {
  // 确保目录存在
  std::filesystem::create_directories(config.output_path.parent_path());

  // 1. 创建 Byte Stream
  wil::com_ptr<IMFByteStream> byte_stream;
  if (FAILED(MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE,
                          config.output_path.c_str(), byte_stream.put()))) {
    return std::unexpected("Failed to create byte stream");
  }

  // 2. 创建 Media Sink
  wil::com_ptr<IMFMediaSink> media_sink;
  if (FAILED(MFCreateMPEG4MediaSink(byte_stream.get(), nullptr, nullptr, media_sink.put()))) {
    return std::unexpected("Failed to create MPEG4 media sink");
  }

  // 3. 创建 Sink Writer 属性并预设编码器参数
  wil::com_ptr<IMFAttributes> attributes;
  if (FAILED(MFCreateAttributes(attributes.put(), 8))) {
    return std::unexpected("Failed to create MF attributes");
  }

  // 启用硬件加速 (仅对编码,输入仍然是 CPU 内存)
  if (FAILED(attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE))) {
    Logger().warn("Failed to enable hardware transforms for encoder");
  }

  // 在创建 Sink Writer 前预设 Rate Control Mode
  UINT32 rate_control_mode = eAVEncCommonRateControlMode_CBR;  // 默认 CBR

  if (config.rate_control == Types::RateControlMode::VBR) {
    rate_control_mode = eAVEncCommonRateControlMode_Quality;
  } else if (config.rate_control == Types::RateControlMode::ManualQP) {
    rate_control_mode = eAVEncCommonRateControlMode_Quality;  // QP 模式也使用 Quality
  }

  if (FAILED(attributes->SetUINT32(CODECAPI_AVEncCommonRateControlMode, rate_control_mode))) {
    Logger().warn("Failed to set rate control mode attribute");
  }

  // 预设 Quality (VBR 模式) 或 QP (ManualQP 模式)
  if (config.rate_control == Types::RateControlMode::VBR) {
    if (FAILED(attributes->SetUINT32(CODECAPI_AVEncCommonQuality, config.quality))) {
      Logger().warn("Failed to set quality attribute");
    }
    Logger().info("CPU encoder configured for VBR mode with quality: {}", config.quality);
  } else if (config.rate_control == Types::RateControlMode::ManualQP) {
    // 尝试设置 QP 参数（可能不被所有编码器支持）
    if (FAILED(attributes->SetUINT32(CODECAPI_AVEncVideoEncodeQP, config.qp))) {
      Logger().warn("Failed to set QP attribute - encoder may not support Manual QP mode");
    }
    Logger().info("CPU encoder configured for Manual QP mode with QP: {}", config.qp);
  } else {
    Logger().info("CPU encoder configured for CBR mode");
  }

  // 4. 用预设属性创建 Sink Writer
  if (FAILED(MFCreateSinkWriterFromMediaSink(media_sink.get(), attributes.get(),
                                             ctx.sink_writer.put()))) {
    return std::unexpected("Failed to create Sink Writer from media sink");
  }

  // 5. 创建输出媒体类型
  auto media_type_out =
      create_output_media_type(config.width, config.height, config.fps, config.bitrate,
                               config.keyframe_interval, config.codec);
  if (!media_type_out) {
    return std::unexpected("Failed to create output media type");
  }

  if (FAILED(ctx.sink_writer->AddStream(media_type_out.get(), &ctx.video_stream_index))) {
    return std::unexpected("Failed to add video stream");
  }

  // 6. 创建输入媒体类型
  auto media_type_in = create_input_media_type(config.width, config.height, config.fps, true);
  if (!media_type_in) {
    return std::unexpected("Failed to create input media type");
  }

  if (FAILED(ctx.sink_writer->SetInputMediaType(ctx.video_stream_index, media_type_in.get(),
                                                nullptr))) {
    return std::unexpected("Failed to set input media type");
  }

  // 7. 如果有音频格式，添加音频流（必须在 BeginWriting 之前）
  if (wave_format) {
    auto audio_result = add_audio_stream(ctx, wave_format, config.audio_bitrate);
    if (!audio_result) {
      Logger().warn("Failed to add audio stream to CPU encoder: {}", audio_result.error());
    }
  }

  // 8. 开始写入
  if (FAILED(ctx.sink_writer->BeginWriting())) {
    return std::unexpected("Failed to begin writing");
  }

  // 缓存尺寸信息
  ctx.frame_width = config.width;
  ctx.frame_height = config.height;
  ctx.buffer_size = config.width * config.height * 4;
  ctx.gpu_encoding = false;
  return {};
}

auto create_encoder(const Types::EncoderConfig& config, ID3D11Device* device,
                    WAVEFORMATEX* wave_format)
    -> std::expected<State::EncoderContext, std::string> {
  auto ctx = std::make_unique<State::EncoderContext>();

  bool try_gpu = (config.encoder_mode == Types::EncoderMode::Auto ||
                  config.encoder_mode == Types::EncoderMode::GPU) &&
                 device != nullptr;

  const char* codec_name = (config.codec == Types::VideoCodec::H265) ? "H.265" : "H.264";

  if (try_gpu) {
    auto gpu_result = try_create_gpu_encoder(*ctx, config, device, wave_format);
    if (gpu_result) {
      Logger().info("GPU encoder created: {}x{} @ {}fps, {} bps, codec: {}", config.width,
                    config.height, config.fps, config.bitrate, codec_name);
      return std::move(*ctx);
    }

    // GPU 失败
    Logger().warn("Failed to create GPU encoder: {}", gpu_result.error());

    if (config.encoder_mode == Types::EncoderMode::GPU) {
      // 强制 GPU 模式，不降级
      return std::unexpected(gpu_result.error());
    }

    // Auto 模式，降级到 CPU
    Logger().info("Falling back to CPU encoder");
    ctx = std::make_unique<State::EncoderContext>();  // 重置 context
  }

  // CPU 编码
  auto cpu_result = create_cpu_encoder(*ctx, config, wave_format);
  if (!cpu_result) {
    return std::unexpected(cpu_result.error());
  }

  Logger().info("CPU encoder created: {}x{} @ {}fps, {} bps, codec: {}", config.width,
                config.height, config.fps, config.bitrate, codec_name);
  return std::move(*ctx);
}

// GPU 编码帧（内部函数）
auto encode_frame_gpu(State::EncoderContext& encoder, ID3D11DeviceContext* context,
                      ID3D11Texture2D* frame_texture, int64_t timestamp_100ns, uint32_t fps)
    -> std::expected<void, std::string> {
  // 1. 复制到共享纹理
  context->CopyResource(encoder.shared_texture.get(), frame_texture);

  // 2. 从 DXGI Surface 创建 MF Buffer
  wil::com_ptr<IMFMediaBuffer> buffer;
  wil::com_ptr<IDXGISurface> surface;

  if (FAILED(encoder.shared_texture->QueryInterface(IID_PPV_ARGS(surface.put())))) {
    return std::unexpected("Failed to query DXGI surface");
  }

  if (FAILED(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), surface.get(), 0, FALSE,
                                       buffer.put()))) {
    return std::unexpected("Failed to create DXGI surface buffer");
  }

  // 设置 buffer 长度（使用缓存的尺寸）
  buffer->SetCurrentLength(encoder.buffer_size);

  // 3. 创建 Sample
  wil::com_ptr<IMFSample> sample;
  if (FAILED(MFCreateSample(sample.put()))) {
    return std::unexpected("Failed to create MF sample");
  }

  sample->AddBuffer(buffer.get());
  sample->SetSampleTime(timestamp_100ns);
  sample->SetSampleDuration(10'000'000 / fps);

  // 4. 写入
  if (FAILED(encoder.sink_writer->WriteSample(encoder.video_stream_index, sample.get()))) {
    return std::unexpected("Failed to write GPU sample");
  }

  return {};
}

// CPU 编码帧（内部函数）
auto encode_frame_cpu(State::EncoderContext& encoder, ID3D11DeviceContext* context,
                      ID3D11Texture2D* frame_texture, int64_t timestamp_100ns, uint32_t fps)
    -> std::expected<void, std::string> {
  // 1. 获取纹理描述
  D3D11_TEXTURE2D_DESC desc;
  frame_texture->GetDesc(&desc);

  // 2. 确保 staging texture 存在且尺寸匹配
  if (!encoder.staging_texture) {
    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.MiscFlags = 0;

    wil::com_ptr<ID3D11Device> device;
    frame_texture->GetDevice(device.put());
    if (FAILED(device->CreateTexture2D(&staging_desc, nullptr, encoder.staging_texture.put()))) {
      return std::unexpected("Failed to create staging texture");
    }
  }

  // 3. 首次使用时创建可复用的 Sample 和 Buffer
  if (!encoder.reusable_sample) {
    if (FAILED(MFCreateSample(encoder.reusable_sample.put()))) {
      return std::unexpected("Failed to create reusable sample");
    }
    if (FAILED(MFCreateMemoryBuffer(encoder.buffer_size, encoder.reusable_buffer.put()))) {
      return std::unexpected("Failed to create reusable buffer");
    }
    encoder.reusable_sample->AddBuffer(encoder.reusable_buffer.get());
  }

  // 4. 复制纹理数据
  context->CopyResource(encoder.staging_texture.get(), frame_texture);

  // 5. 映射 staging texture 读取数据
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (FAILED(context->Map(encoder.staging_texture.get(), 0, D3D11_MAP_READ, 0, &mapped))) {
    return std::unexpected("Failed to map staging texture");
  }

  // 6. 将数据写入复用的 buffer
  BYTE* dest = nullptr;
  HRESULT hr = encoder.reusable_buffer->Lock(&dest, nullptr, nullptr);
  if (SUCCEEDED(hr)) {
    const BYTE* src = static_cast<const BYTE*>(mapped.pData);
    const UINT row_pitch = encoder.frame_width * 4;

    if (row_pitch == mapped.RowPitch) {
      // 直接复制
      std::memcpy(dest, src, encoder.buffer_size);
    } else {
      // 逐行复制
      for (UINT y = 0; y < encoder.frame_height; ++y) {
        std::memcpy(dest + y * row_pitch, src + y * mapped.RowPitch, row_pitch);
      }
    }

    encoder.reusable_buffer->Unlock();
    encoder.reusable_buffer->SetCurrentLength(encoder.buffer_size);

    // 7. 设置时间戳并写入
    encoder.reusable_sample->SetSampleTime(timestamp_100ns);
    encoder.reusable_sample->SetSampleDuration(10'000'000 / fps);
    hr =
        encoder.sink_writer->WriteSample(encoder.video_stream_index, encoder.reusable_sample.get());
  }

  context->Unmap(encoder.staging_texture.get(), 0);

  if (FAILED(hr)) {
    return std::unexpected("Failed to write CPU sample");
  }
  return {};
}

auto encode_frame(State::EncoderContext& encoder, ID3D11DeviceContext* context,
                  ID3D11Texture2D* frame_texture, int64_t timestamp_100ns, uint32_t fps)
    -> std::expected<void, std::string> {
  if (!encoder.sink_writer || !context || !frame_texture) {
    return std::unexpected("Invalid encoder state");
  }

  // 注：线程同步由调用方管理
  if (encoder.gpu_encoding) {
    return encode_frame_gpu(encoder, context, frame_texture, timestamp_100ns, fps);
  } else {
    return encode_frame_cpu(encoder, context, frame_texture, timestamp_100ns, fps);
  }
}

auto encode_audio(State::EncoderContext& encoder, const BYTE* audio_data, UINT32 num_frames,
                  UINT32 bytes_per_frame, int64_t timestamp_100ns)
    -> std::expected<void, std::string> {
  if (!encoder.sink_writer || !encoder.has_audio) {
    return std::unexpected("Audio stream not available");
  }

  if (!audio_data || num_frames == 0) {
    return {};  // 空数据，跳过
  }

  UINT32 data_size = num_frames * bytes_per_frame;

  // 创建音频 buffer
  wil::com_ptr<IMFMediaBuffer> buffer;
  if (FAILED(MFCreateMemoryBuffer(data_size, buffer.put()))) {
    return std::unexpected("Failed to create audio buffer");
  }

  // 复制音频数据
  BYTE* buffer_data = nullptr;
  if (FAILED(buffer->Lock(&buffer_data, nullptr, nullptr))) {
    return std::unexpected("Failed to lock audio buffer");
  }

  std::memcpy(buffer_data, audio_data, data_size);
  buffer->Unlock();
  buffer->SetCurrentLength(data_size);

  // 创建音频 sample
  wil::com_ptr<IMFSample> sample;
  if (FAILED(MFCreateSample(sample.put()))) {
    return std::unexpected("Failed to create audio sample");
  }

  sample->AddBuffer(buffer.get());
  sample->SetSampleTime(timestamp_100ns);

  // 注：线程同步由调用方管理
  if (FAILED(encoder.sink_writer->WriteSample(encoder.audio_stream_index, sample.get()))) {
    return std::unexpected("Failed to write audio sample");
  }

  return {};
}

auto finalize_encoder(State::EncoderContext& encoder) -> std::expected<void, std::string> {
  if (encoder.sink_writer) {
    if (FAILED(encoder.sink_writer->Finalize())) {
      return std::unexpected("Failed to finalize sink writer");
    }
    encoder.sink_writer = nullptr;
  }

  // 清理所有资源
  encoder.staging_texture = nullptr;
  encoder.reusable_sample = nullptr;
  encoder.reusable_buffer = nullptr;
  encoder.shared_texture = nullptr;
  encoder.dxgi_manager = nullptr;

  return {};
}

}  // namespace Utils::Media::Encoder
