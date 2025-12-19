module;

#include <mfidl.h>
#include <mfreadwrite.h>

module Features.Recording.Encoder;

import std;
import Utils.Logger;
import <d3d11.h>;
import <mfapi.h>;
import <mferror.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Recording::Encoder {

// 辅助函数：创建输出媒体类型
auto create_output_media_type(uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate)
    -> wil::com_ptr<IMFMediaType> {
  wil::com_ptr<IMFMediaType> media_type;
  if (FAILED(MFCreateMediaType(media_type.put()))) return nullptr;
  if (FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))) return nullptr;
  if (FAILED(media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264))) return nullptr;
  if (FAILED(media_type->SetUINT32(MF_MT_AVG_BITRATE, bitrate))) return nullptr;
  if (FAILED(media_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)))
    return nullptr;
  if (FAILED(MFSetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, width, height))) return nullptr;
  if (FAILED(MFSetAttributeRatio(media_type.get(), MF_MT_FRAME_RATE, fps, 1))) return nullptr;
  if (FAILED(MFSetAttributeRatio(media_type.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1))) return nullptr;
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
  return media_type;
}

// 尝试创建 GPU 编码器
auto try_create_gpu_encoder(Features::Recording::State::EncoderContext& ctx,
                            const std::filesystem::path& output_path, uint32_t width,
                            uint32_t height, uint32_t fps, uint32_t bitrate, ID3D11Device* device)
    -> std::expected<void, std::string> {
  // 1. 创建 DXGI Device Manager
  if (FAILED(MFCreateDXGIDeviceManager(&ctx.reset_token, ctx.dxgi_manager.put()))) {
    return std::unexpected("Failed to create DXGI Device Manager");
  }

  // 2. 将 D3D11 设备关联到 DXGI Manager
  if (FAILED(ctx.dxgi_manager->ResetDevice(device, ctx.reset_token))) {
    return std::unexpected("Failed to reset DXGI device");
  }

  // 3. 创建 Sink Writer 属性
  wil::com_ptr<IMFAttributes> attributes;
  if (FAILED(MFCreateAttributes(attributes.put(), 3))) {
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

  // 确保目录存在
  std::filesystem::create_directories(output_path.parent_path());

  // 4. 创建 Sink Writer
  if (FAILED(MFCreateSinkWriterFromURL(output_path.c_str(), nullptr, attributes.get(),
                                       ctx.sink_writer.put()))) {
    return std::unexpected("Failed to create Sink Writer with GPU support");
  }

  // 5. 创建输出媒体类型
  auto media_type_out = create_output_media_type(width, height, fps, bitrate);
  if (!media_type_out) {
    return std::unexpected("Failed to create output media type");
  }

  if (FAILED(ctx.sink_writer->AddStream(media_type_out.get(), &ctx.video_stream_index))) {
    return std::unexpected("Failed to add video stream");
  }

  // 6. 创建 GPU 输入媒体类型
  auto media_type_in = create_input_media_type(width, height, fps, false);
  if (!media_type_in) {
    return std::unexpected("Failed to create GPU input media type");
  }

  if (FAILED(ctx.sink_writer->SetInputMediaType(ctx.video_stream_index, media_type_in.get(),
                                                nullptr))) {
    return std::unexpected("Failed to set GPU input media type");
  }

  // 7. 创建共享纹理 (编码器专用)
  D3D11_TEXTURE2D_DESC tex_desc = {};
  tex_desc.Width = width;
  tex_desc.Height = height;
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

  // 8. 开始写入
  if (FAILED(ctx.sink_writer->BeginWriting())) {
    return std::unexpected("Failed to begin writing with GPU encoder");
  }

  // 缓存尺寸信息
  ctx.frame_width = width;
  ctx.frame_height = height;
  ctx.buffer_size = width * height * 4;
  ctx.gpu_encoding = true;
  return {};
}

// 创建 CPU 编码器 (fallback)
auto create_cpu_encoder(Features::Recording::State::EncoderContext& ctx,
                        const std::filesystem::path& output_path, uint32_t width, uint32_t height,
                        uint32_t fps, uint32_t bitrate) -> std::expected<void, std::string> {
  // 创建 Sink Writer 属性
  wil::com_ptr<IMFAttributes> attributes;
  if (FAILED(MFCreateAttributes(attributes.put(), 1))) {
    return std::unexpected("Failed to create MF attributes");
  }

  // 启用硬件加速 (仅对编码，输入仍然是 CPU 内存)
  if (FAILED(attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE))) {
    Logger().warn("Failed to enable hardware transforms for encoder");
  }

  // 确保目录存在
  std::filesystem::create_directories(output_path.parent_path());

  // 创建 Sink Writer
  if (FAILED(MFCreateSinkWriterFromURL(output_path.c_str(), nullptr, attributes.get(),
                                       ctx.sink_writer.put()))) {
    return std::unexpected("Failed to create Sink Writer");
  }

  // 创建输出媒体类型
  auto media_type_out = create_output_media_type(width, height, fps, bitrate);
  if (!media_type_out) {
    return std::unexpected("Failed to create output media type");
  }

  if (FAILED(ctx.sink_writer->AddStream(media_type_out.get(), &ctx.video_stream_index))) {
    return std::unexpected("Failed to add video stream");
  }

  // 创建输入媒体类型
  auto media_type_in = create_input_media_type(width, height, fps, true);
  if (!media_type_in) {
    return std::unexpected("Failed to create input media type");
  }

  if (FAILED(ctx.sink_writer->SetInputMediaType(ctx.video_stream_index, media_type_in.get(),
                                                nullptr))) {
    return std::unexpected("Failed to set input media type");
  }

  if (FAILED(ctx.sink_writer->BeginWriting())) {
    return std::unexpected("Failed to begin writing");
  }

  // 缓存尺寸信息
  ctx.frame_width = width;
  ctx.frame_height = height;
  ctx.buffer_size = width * height * 4;
  ctx.gpu_encoding = false;
  return {};
}

auto create_encoder(const std::filesystem::path& output_path, uint32_t width, uint32_t height,
                    uint32_t fps, uint32_t bitrate, ID3D11Device* device,
                    Features::Recording::Types::EncoderMode mode)
    -> std::expected<Features::Recording::State::EncoderContext, std::string> {
  Features::Recording::State::EncoderContext ctx;

  bool try_gpu = (mode == Features::Recording::Types::EncoderMode::Auto ||
                  mode == Features::Recording::Types::EncoderMode::GPU) &&
                 device != nullptr;

  if (try_gpu) {
    auto gpu_result = try_create_gpu_encoder(ctx, output_path, width, height, fps, bitrate, device);
    if (gpu_result) {
      Logger().info("GPU encoder created: {}x{} @ {}fps, {} bps", width, height, fps, bitrate);
      return ctx;
    }

    // GPU 失败
    Logger().warn("Failed to create GPU encoder: {}", gpu_result.error());

    if (mode == Features::Recording::Types::EncoderMode::GPU) {
      // 强制 GPU 模式，不降级
      return std::unexpected(gpu_result.error());
    }

    // Auto 模式，降级到 CPU
    Logger().info("Falling back to CPU encoder");
    ctx = {};  // 重置 context
  }

  // CPU 编码
  auto cpu_result = create_cpu_encoder(ctx, output_path, width, height, fps, bitrate);
  if (!cpu_result) {
    return std::unexpected(cpu_result.error());
  }

  Logger().info("CPU encoder created: {}x{} @ {}fps, {} bps", width, height, fps, bitrate);
  return ctx;
}

// GPU 编码帧
auto encode_frame_gpu(Features::Recording::State::EncoderContext& encoder,
                      ID3D11DeviceContext* context, ID3D11Texture2D* frame_texture,
                      int64_t timestamp_100ns, uint32_t fps) -> std::expected<void, std::string> {
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

// CPU 编码帧
auto encode_frame_cpu(Features::Recording::State::EncoderContext& encoder,
                      ID3D11DeviceContext* context, ID3D11Texture2D* frame_texture,
                      int64_t timestamp_100ns, uint32_t fps) -> std::expected<void, std::string> {
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

auto encode_frame(Features::Recording::State::EncoderContext& encoder, ID3D11DeviceContext* context,
                  ID3D11Texture2D* frame_texture, int64_t timestamp_100ns, uint32_t fps)
    -> std::expected<void, std::string> {
  if (!encoder.sink_writer || !context || !frame_texture) {
    return std::unexpected("Invalid encoder state");
  }

  if (encoder.gpu_encoding) {
    return encode_frame_gpu(encoder, context, frame_texture, timestamp_100ns, fps);
  } else {
    return encode_frame_cpu(encoder, context, frame_texture, timestamp_100ns, fps);
  }
}

auto finalize_encoder(Features::Recording::State::EncoderContext& encoder)
    -> std::expected<void, std::string> {
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

}  // namespace Features::Recording::Encoder
