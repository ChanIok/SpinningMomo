#include "features/screenshot/hdr_encoder.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/wincodec.hpp"

#include "utils/image/image.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::screenshot::hdr_encoder {

namespace {

auto check_hresult(HRESULT hr, std::string_view operation) -> std::expected<void, std::string> {
  if (FAILED(hr)) {
    return std::unexpected(
        std::format("{} HRESULT=0x{:08X}", operation, static_cast<unsigned>(hr)));
  }
  return {};
}

// 在目标路径后拼出唯一临时文件名：.tmp-<进程ID>-<线程ID>-<自增序号>
auto make_temporary_path(const std::filesystem::path& destination) -> std::filesystem::path {
  // 原子自增序号保证多线程并发截图时临时文件名互不冲突
  static std::atomic<std::uint64_t> next_id = 0;

  auto temporary_path = destination;
  temporary_path += std::format(L".tmp-{}-{}-{}", GetCurrentProcessId(), GetCurrentThreadId(),
                                next_id.fetch_add(1, std::memory_order_relaxed));
  return temporary_path;
}

// 原子抢占一个不存在的临时文件路径并占位，撞名就换序号重试，最多 32 次
auto reserve_temporary_path(const std::filesystem::path& destination)
    -> std::expected<std::filesystem::path, std::string> {
  for (int attempt = 0; attempt < 32; ++attempt) {
    const auto temporary_path = make_temporary_path(destination);
    // CREATE_NEW 仅在文件不存在时创建成功，借此原子占位，避免并发写同一文件
    const auto handle = CreateFileW(temporary_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                    nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
      // 占位成功后立即关闭句柄，真正的像素写入由后续 WIC 重新打开该文件
      if (!CloseHandle(handle)) {
        std::error_code error;
        std::filesystem::remove(temporary_path, error);
        return std::unexpected("Failed to close temporary JPEG XR output file");
      }
      return temporary_path;
    }

    const auto error = GetLastError();
    // 文件已存在属于撞名，换序号重试；其余 Win32 错误直接失败
    if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
      return std::unexpected(
          std::format("Failed to create temporary JPEG XR output file (Win32 error {})",
                      static_cast<unsigned long>(error)));
    }
  }

  // 连续 32 次撞名，判定无法分配唯一临时路径
  return std::unexpected("Failed to allocate a unique temporary JPEG XR output path");
}

}  // namespace

// 把捕获纹理复制到 staging texture，但把 Map 延后到调用方决定的时机。
auto begin_jxr_readback(ID3D11Texture2D* texture)
    -> std::expected<JxrReadbackSession, std::string> {
  if (!texture) {
    return std::unexpected("Texture cannot be null");
  }

  D3D11_TEXTURE2D_DESC desc{};
  texture->GetDesc(&desc);
  // 必须是 half float 格式，像素内存布局才能与 WIC 的 64bppRGBAHalf 零转换透传。
  if (desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) {
    return std::unexpected("JPEG XR input texture must use R16G16B16A16_FLOAT");
  }
  if (desc.Width == 0 || desc.Height == 0) {
    return std::unexpected("JPEG XR input texture has empty dimensions");
  }
  // 单张、无 mip、无多重采样的 2D 纹理才有确定的 staging 读回布局。
  if (desc.ArraySize != 1 || desc.MipLevels != 1 || desc.SampleDesc.Count != 1) {
    return std::unexpected("JPEG XR input texture must be a single-sample 2D texture");
  }

  try {
    wil::com_ptr<ID3D11Device> device;
    texture->GetDevice(device.put());
    THROW_HR_IF_NULL(E_POINTER, device);

    wil::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    THROW_HR_IF_NULL(E_POINTER, context);

    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;

    wil::com_ptr<ID3D11Texture2D> staging_texture;
    if (auto result =
            check_hresult(device->CreateTexture2D(&staging_desc, nullptr, staging_texture.put()),
                          "Failed to create JPEG XR staging texture");
        !result) {
      return std::unexpected(result.error());
    }

    // 只提交 GPU copy，不在这里 Map；调用方可以先提交 Ultra HDR 的 GPU 工作。
    context->CopyResource(staging_texture.get(), texture);

    return JxrReadbackSession{
        .context = std::move(context),
        .staging_texture = std::move(staging_texture),
        .width = desc.Width,
        .height = desc.Height,
    };
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("JPEG XR readback setup failed: {}", e.what()));
  } catch (const std::exception& e) {
    return std::unexpected(std::format("JPEG XR readback setup failed: {}", e.what()));
  } catch (...) {
    return std::unexpected("JPEG XR readback setup failed");
  }
}

// Map staging texture，并复制成紧排 CPU 缓冲，之后后台线程不再接触 D3D 资源。
auto read_jxr_pixels(JxrReadbackSession session) -> std::expected<JxrPixelData, std::string> {
  if (!session.context || !session.staging_texture || session.width == 0 || session.height == 0) {
    return std::unexpected("Invalid JPEG XR readback session");
  }

  constexpr std::uint64_t kBytesPerPixel = 8;
  const auto row_bytes = static_cast<std::uint64_t>(session.width) * kBytesPerPixel;
  if (row_bytes > std::numeric_limits<std::uint32_t>::max() ||
      row_bytes > std::numeric_limits<std::uint64_t>::max() / session.height) {
    return std::unexpected("JPEG XR pixel buffer is too large");
  }
  const auto pixel_bytes = row_bytes * session.height;
  if (pixel_bytes > std::numeric_limits<std::size_t>::max()) {
    return std::unexpected("JPEG XR pixel buffer is too large");
  }

  try {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (auto result = check_hresult(
            session.context->Map(session.staging_texture.get(), 0, D3D11_MAP_READ, 0, &mapped),
            "Failed to map JPEG XR staging texture");
        !result) {
      return std::unexpected(result.error());
    }
    auto unmap_on_exit =
        wil::scope_exit([&session] { session.context->Unmap(session.staging_texture.get(), 0); });

    if (!mapped.pData || mapped.RowPitch < row_bytes) {
      return std::unexpected("JPEG XR mapped texture row pitch is invalid");
    }

    JxrPixelData result{
        .pixels = std::vector<std::uint8_t>(static_cast<std::size_t>(pixel_bytes)),
        .width = session.width,
        .height = session.height,
        .row_pitch = static_cast<std::uint32_t>(row_bytes),
    };
    for (std::uint32_t row = 0; row < session.height; ++row) {
      const auto* source = static_cast<const std::uint8_t*>(mapped.pData) +
                           static_cast<std::size_t>(row) * mapped.RowPitch;
      auto* destination = result.pixels.data() + static_cast<std::size_t>(row) * result.row_pitch;
      std::memcpy(destination, source, result.row_pitch);
    }

    return result;
  } catch (const wil::ResultException& e) {
    return std::unexpected(std::format("JPEG XR pixel readback failed: {}", e.what()));
  } catch (const std::exception& e) {
    return std::unexpected(std::format("JPEG XR pixel readback failed: {}", e.what()));
  } catch (...) {
    return std::unexpected("JPEG XR pixel readback failed");
  }
}

namespace {

// 把脱离 D3D 的 FP16 像素经 WIC 无损编码为 JPEG XR，像素直接写入指定临时文件。
auto encode_pixels_to_jxr_file(const JxrPixelData& pixels,
                               const std::filesystem::path& temporary_path)
    -> std::expected<void, std::string> {
  constexpr std::uint64_t kBytesPerPixel = 8;
  if (pixels.width == 0 || pixels.height == 0 || pixels.row_pitch == 0 || pixels.pixels.empty()) {
    return std::unexpected("JPEG XR pixel data is empty");
  }

  const auto expected_row_pitch = static_cast<std::uint64_t>(pixels.width) * kBytesPerPixel;
  if (expected_row_pitch > std::numeric_limits<std::uint64_t>::max() / pixels.height) {
    return std::unexpected("JPEG XR pixel data is too large");
  }
  const auto expected_size = expected_row_pitch * pixels.height;
  if (pixels.row_pitch != expected_row_pitch || pixels.pixels.size() != expected_size ||
      expected_size > std::numeric_limits<UINT>::max()) {
    return std::unexpected("JPEG XR pixel data layout is invalid");
  }

  // WIC 工厂和编码器只在当前线程使用；后台任务会为自己建立 thread-local 工厂。
  auto factory_result = utils::image::get_thread_wic_factory();
  if (!factory_result) {
    return std::unexpected("Failed to create WIC imaging factory: " + factory_result.error());
  }
  auto factory = factory_result.value();

  wil::com_ptr<IWICBitmapEncoder> encoder;
  if (auto result =
          check_hresult(factory->CreateEncoder(GUID_ContainerFormatWmp, nullptr, encoder.put()),
                        "Failed to create JPEG XR encoder");
      !result) {
    return std::unexpected(result.error());
  }

  wil::com_ptr<IWICBitmapEncoderInfo> encoder_info;
  if (auto result = check_hresult(encoder->GetEncoderInfo(encoder_info.put()),
                                  "Failed to query JPEG XR encoder info");
      !result) {
    return std::unexpected(result.error());
  }
  BOOL supports_lossless = FALSE;
  if (auto result = check_hresult(encoder_info->DoesSupportLossless(&supports_lossless),
                                  "Failed to query JPEG XR lossless support");
      !result) {
    return std::unexpected(result.error());
  }
  if (!supports_lossless) {
    return std::unexpected("JPEG XR encoder does not support lossless encoding");
  }

  wil::com_ptr<IWICStream> stream;
  if (auto result =
          check_hresult(factory->CreateStream(stream.put()), "Failed to create JPEG XR stream");
      !result) {
    return std::unexpected(result.error());
  }
  if (auto result =
          check_hresult(stream->InitializeFromFilename(temporary_path.c_str(), GENERIC_WRITE),
                        "Failed to open JPEG XR output stream");
      !result) {
    return std::unexpected(result.error());
  }
  if (auto result = check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache),
                                  "Failed to initialize JPEG XR encoder");
      !result) {
    return std::unexpected(result.error());
  }

  wil::com_ptr<IWICBitmapFrameEncode> frame;
  wil::com_ptr<IPropertyBag2> property_bag;
  if (auto result = check_hresult(encoder->CreateNewFrame(frame.put(), property_bag.put()),
                                  "Failed to create JPEG XR frame");
      !result) {
    return std::unexpected(result.error());
  }
  if (!property_bag) {
    return std::unexpected("JPEG XR encoder did not provide an options property bag");
  }

  PROPBAG2 lossless_option{};
  lossless_option.pstrName = const_cast<LPOLESTR>(L"Lossless");
  VARIANT lossless_value{};
  VariantInit(&lossless_value);
  lossless_value.vt = VT_BOOL;
  lossless_value.boolVal = VARIANT_TRUE;
  if (auto result = check_hresult(property_bag->Write(1, &lossless_option, &lossless_value),
                                  "Failed to enable JPEG XR lossless encoding");
      !result) {
    return std::unexpected(result.error());
  }

  // AlphaQuality=1：JPEG XR 的 alpha 是独立平面，1 表示 alpha 通道同样无损。
  PROPBAG2 alpha_quality_option{};
  alpha_quality_option.pstrName = const_cast<LPOLESTR>(L"AlphaQuality");
  VARIANT alpha_quality_value{};
  VariantInit(&alpha_quality_value);
  alpha_quality_value.vt = VT_UI1;
  alpha_quality_value.bVal = 1;
  if (auto result =
          check_hresult(property_bag->Write(1, &alpha_quality_option, &alpha_quality_value),
                        "Failed to enable JPEG XR lossless alpha encoding");
      !result) {
    return std::unexpected(result.error());
  }

  if (auto result = check_hresult(frame->Initialize(property_bag.get()),
                                  "Failed to initialize JPEG XR frame");
      !result) {
    return std::unexpected(result.error());
  }
  if (auto result = check_hresult(frame->SetSize(pixels.width, pixels.height),
                                  "Failed to set JPEG XR frame size");
      !result) {
    return std::unexpected(result.error());
  }

  WICPixelFormatGUID pixel_format = GUID_WICPixelFormat64bppRGBAHalf;
  if (auto result =
          check_hresult(frame->SetPixelFormat(&pixel_format), "Failed to set JPEG XR pixel format");
      !result) {
    return std::unexpected(result.error());
  }
  if (!IsEqualGUID(pixel_format, GUID_WICPixelFormat64bppRGBAHalf)) {
    return std::unexpected("JPEG XR encoder did not accept 64bppRGBAHalf input");
  }

  auto* pixel_data = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(pixels.pixels.data()));
  if (auto result =
          check_hresult(frame->WritePixels(pixels.height, pixels.row_pitch,
                                           static_cast<UINT>(pixels.pixels.size()), pixel_data),
                        "Failed to write JPEG XR pixels");
      !result) {
    return std::unexpected(result.error());
  }
  if (auto result = check_hresult(frame->Commit(), "Failed to commit JPEG XR frame"); !result) {
    return std::unexpected(result.error());
  }
  if (auto result = check_hresult(encoder->Commit(), "Failed to commit JPEG XR encoder"); !result) {
    return std::unexpected(result.error());
  }

  return {};
}

}  // namespace

// 保存脱离 D3D 的 JPEG XR 像素：抢占临时文件 → 无损编码 → 原子改名替换目标文件。
auto save_jxr_pixels(const JxrPixelData& pixels, const std::wstring& file_path)
    -> std::expected<void, std::string> {
  if (file_path.empty()) {
    return std::unexpected("JPEG XR output path is empty");
  }

  const std::filesystem::path destination(file_path);
  // 先抢占唯一临时文件，编码全程只写临时文件，避免半成品出现在目标路径
  auto temporary_path_result = reserve_temporary_path(destination);
  if (!temporary_path_result) {
    return std::unexpected(temporary_path_result.error());
  }
  const auto temporary_path = std::move(temporary_path_result.value());
  // 失败兜底：只要改名未完成，函数退出时一律删除临时文件
  bool temporary_created = true;
  auto cleanup_temporary = wil::scope_exit([&] {
    if (temporary_created) {
      std::error_code error;
      std::filesystem::remove(temporary_path, error);
    }
  });

  // 执行编码并统计耗时；失败时由上面的 scope_exit 负责清理临时文件。
  const auto encode_start = std::chrono::steady_clock::now();
  auto encode_result = encode_pixels_to_jxr_file(pixels, temporary_path);
  const auto encode_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - encode_start)
                             .count();
  if (!encode_result) {
    return std::unexpected(encode_result.error());
  }

  // 编码成功后原子替换目标文件；WRITE_THROUGH 保证函数返回前数据已落盘
  if (!MoveFileExW(temporary_path.c_str(), destination.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    return std::unexpected(std::format("Failed to replace JPEG XR output file (Win32 error {})",
                                       static_cast<unsigned long>(GetLastError())));
  }

  // 改名成功，解除临时文件清理并记录日志
  temporary_created = false;
  Logger().info("HDR JPEG XR saved: path={}, encode={} ms", utils::string::ToUtf8(file_path),
                encode_ms);
  return {};
}

}  // namespace features::screenshot::hdr_encoder
