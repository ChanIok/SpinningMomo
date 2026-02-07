module;

#include <wil/com.h>

module Utils.Image;

import std;
import Utils.Logger;
import <shlwapi.h>;
import <webp/encode.h>;
import <webp/types.h>;
import <wincodec.h>;
import <windows.h>;
import <winerror.h>;

namespace Utils::Image {

// 格式化HRESULT错误信息
auto format_hresult(HRESULT hr, const std::string& context) -> std::string {
  char* message_buffer = nullptr;
  auto size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, static_cast<DWORD>(hr), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

  std::string message;
  if (size > 0 && message_buffer) {
    message = std::format("{} (HRESULT: 0x{:08X}): {}", context, static_cast<unsigned int>(hr),
                          message_buffer);
    LocalFree(message_buffer);
  } else {
    message = std::format("{} (HRESULT: 0x{:08X})", context, static_cast<unsigned int>(hr));
  }
  return message;
}

// 获取MIME类型
auto get_mime_type(IWICBitmapDecoder* decoder) -> std::string {
  GUID container_format;
  if (FAILED(decoder->GetContainerFormat(&container_format))) {
    return "application/octet-stream";
  }

  if (IsEqualGUID(container_format, GUID_ContainerFormatBmp)) {
    return "image/bmp";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatPng)) {
    return "image/png";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatIco)) {
    return "image/x-icon";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatJpeg)) {
    return "image/jpeg";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatGif)) {
    return "image/gif";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatTiff)) {
    return "image/tiff";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatWmp)) {
    return "image/vnd.ms-photo";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatDds)) {
    return "image/vnd.ms-dds";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatAdng)) {
    return "image/apng";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatHeif)) {
    return "image/heif";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatWebp)) {
    return "image/webp";
  }
  if (IsEqualGUID(container_format, GUID_ContainerFormatRaw)) {
    return "image/x-raw";
  }

  return "application/octet-stream";
}

// 计算缩放尺寸（按短边等比例缩放）
auto calculate_scaled_size(uint32_t original_width, uint32_t original_height,
                           uint32_t short_edge_size) -> std::pair<uint32_t, uint32_t> {
  // 判断哪边是短边
  uint32_t short_edge = std::min(original_width, original_height);

  // 如果短边已经小于或等于目标尺寸，不缩放
  if (short_edge <= short_edge_size) {
    return {original_width, original_height};
  }

  // 计算缩放比例（基于短边）
  double scale = static_cast<double>(short_edge_size) / short_edge;

  // 等比例计算两边
  uint32_t new_width = static_cast<uint32_t>(original_width * scale);
  uint32_t new_height = static_cast<uint32_t>(original_height * scale);

  // 确保至少为1像素
  new_width = std::max(new_width, 1u);
  new_height = std::max(new_height, 1u);

  return {new_width, new_height};
}

// 创建WIC工厂
auto create_factory() -> std::expected<WICFactory, std::string> {
  try {
    auto factory = wil::CoCreateInstance<IWICImagingFactory>(CLSID_WICImagingFactory);
    return factory;
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "Failed to create WIC factory"));
  }
}

// 获取当前线程的WIC工厂
auto get_thread_wic_factory() -> std::expected<WICFactory, std::string> {
  if (!thread_wic_factory) {
    if (!thread_com_init.has_value()) {
      thread_com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);
    }

    auto factory_result = create_factory();
    if (!factory_result) {
      return std::unexpected(factory_result.error());
    }
    thread_wic_factory = std::move(factory_result.value());
  }

  return thread_wic_factory;
}

// 获取图像信息
auto get_image_info(IWICImagingFactory* factory, const std::filesystem::path& path)
    -> std::expected<ImageInfo, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!std::filesystem::exists(path)) {
    return std::unexpected("File does not exist: " + path.string());
  }

  try {
    // 创建解码器
    wil::com_ptr<IWICBitmapDecoder> decoder;
    THROW_IF_FAILED(factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.put()));

    // 获取帧
    wil::com_ptr<IWICBitmapFrameDecode> frame;
    THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));

    // 获取尺寸
    UINT width, height;
    THROW_IF_FAILED(frame->GetSize(&width, &height));

    // 获取像素格式
    GUID pixel_format;
    THROW_IF_FAILED(frame->GetPixelFormat(&pixel_format));

    // 获取MIME类型
    auto mime_type = get_mime_type(decoder.get());

    return ImageInfo{static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixel_format,
                     mime_type};
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

// 加载图像帧
auto load_bitmap_frame(IWICImagingFactory* factory, const std::filesystem::path& path)
    -> std::expected<wil::com_ptr<IWICBitmapFrameDecode>, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!std::filesystem::exists(path)) {
    return std::unexpected("File does not exist: " + path.string());
  }

  try {
    // 创建解码器
    wil::com_ptr<IWICBitmapDecoder> decoder;
    THROW_IF_FAILED(factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.put()));

    // 获取帧
    wil::com_ptr<IWICBitmapFrameDecode> frame;
    THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));

    return frame;
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

// 转换为WIC位图
auto convert_to_bitmap(IWICImagingFactory* factory, IWICBitmapSource* source)
    -> std::expected<wil::com_ptr<IWICBitmap>, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!source) {
    return std::unexpected("Source bitmap is null");
  }

  try {
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(factory->CreateBitmapFromSource(source, WICBitmapCacheOnDemand, bitmap.put()));

    return bitmap;
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

// 缩放图像
auto scale_bitmap(IWICImagingFactory* factory, IWICBitmapSource* source, uint32_t short_edge_size)
    -> std::expected<wil::com_ptr<IWICBitmap>, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!source) {
    return std::unexpected("Source bitmap is null");
  }

  try {
    // 获取原始尺寸
    UINT original_width, original_height;
    THROW_IF_FAILED(source->GetSize(&original_width, &original_height));

    // 计算缩放后尺寸
    auto [new_width, new_height] =
        calculate_scaled_size(original_width, original_height, short_edge_size);

    // 如果尺寸相同，直接转换
    if (new_width == original_width && new_height == original_height) {
      return convert_to_bitmap(factory, source);
    }

    // 创建缩放器
    wil::com_ptr<IWICBitmapScaler> scaler;
    THROW_IF_FAILED(factory->CreateBitmapScaler(scaler.put()));

    // 初始化缩放器
    THROW_IF_FAILED(
        scaler->Initialize(source, new_width, new_height, WICBitmapInterpolationModeFant));

    // 转换为位图
    return convert_to_bitmap(factory, scaler.get());
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

// 将WIC位图编码为WebP
auto encode_bitmap_to_webp(IWICBitmap* bitmap, const WebPEncodeOptions& options)
    -> std::expected<WebPEncodedResult, std::string> {
  if (!bitmap) {
    return std::unexpected("Bitmap is null");
  }

  try {
    // 获取位图尺寸
    UINT width, height;
    THROW_IF_FAILED(bitmap->GetSize(&width, &height));

    // 进行格式转换，确保是BGRA格式
    wil::com_ptr<IWICImagingFactory> factory;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(&factory)));

    wil::com_ptr<IWICFormatConverter> converter;
    THROW_IF_FAILED(factory->CreateFormatConverter(converter.put()));

    THROW_IF_FAILED(converter->Initialize(bitmap, GUID_WICPixelFormat32bppBGRA,
                                          WICBitmapDitherTypeNone, nullptr, 0.0,
                                          WICBitmapPaletteTypeCustom));

    wil::com_ptr<IWICBitmap> bgra_bitmap;
    THROW_IF_FAILED(factory->CreateBitmapFromSource(converter.get(), WICBitmapCacheOnDemand,
                                                    bgra_bitmap.put()));
    bitmap = bgra_bitmap.get();

    // 获取位图锁
    WICRect rect = {0, 0, static_cast<INT>(width), static_cast<INT>(height)};
    wil::com_ptr<IWICBitmapLock> bitmap_lock;
    THROW_IF_FAILED(bitmap->Lock(&rect, WICBitmapLockRead, bitmap_lock.put()));

    // 获取数据指针
    UINT stride, datasize;
    BYTE* data;
    THROW_IF_FAILED(bitmap_lock->GetDataPointer(&datasize, &data));
    THROW_IF_FAILED(bitmap_lock->GetStride(&stride));

    // 编码为WebP
    uint8_t* output = nullptr;
    size_t output_size = 0;

    if (options.lossless) {
      output_size = WebPEncodeLosslessBGRA(data, width, height, stride, &output);
    } else {
      output_size = WebPEncodeBGRA(data, width, height, stride, options.quality, &output);
    }

    if (output_size == 0 || output == nullptr) {
      return std::unexpected("Failed to encode WebP image");
    }

    // 将结果复制到vector
    std::vector<uint8_t> result_data(output, output + output_size);
    WebPFree(output);

    return WebPEncodedResult{std::move(result_data), static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height)};
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

// 直接从文件生成WebP缩略图
auto generate_webp_thumbnail(WICFactory& factory, const std::filesystem::path& path,
                             uint32_t short_edge_size, const WebPEncodeOptions& options)
    -> std::expected<WebPEncodedResult, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  // 加载图像帧
  auto frame_result = load_bitmap_frame(factory.get(), path);
  if (!frame_result) {
    return std::unexpected(frame_result.error());
  }
  auto frame = std::move(frame_result.value());

  // 缩放图像
  auto scaled_result = scale_bitmap(factory.get(), frame.get(), short_edge_size);
  if (!scaled_result) {
    return std::unexpected(scaled_result.error());
  }
  auto scaled_bitmap = std::move(scaled_result.value());

  // 编码为WebP
  return encode_bitmap_to_webp(scaled_bitmap.get(), options);
}

// 保存像素数据到文件
auto save_pixel_data_to_file(IWICImagingFactory* factory, const uint8_t* pixel_data, uint32_t width,
                             uint32_t height, uint32_t row_pitch, const std::wstring& file_path,
                             ImageFormat format, float jpeg_quality)
    -> std::expected<void, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!pixel_data) {
    return std::unexpected("Pixel data is null");
  }

  try {
    // 根据格式选择编码器 GUID
    GUID container_format =
        (format == ImageFormat::JPEG) ? GUID_ContainerFormatJpeg : GUID_ContainerFormatPng;

    wil::com_ptr<IWICBitmapEncoder> encoder;
    THROW_IF_FAILED(factory->CreateEncoder(container_format, nullptr, encoder.put()));

    // 创建流
    wil::com_ptr<IWICStream> stream;
    THROW_IF_FAILED(factory->CreateStream(stream.put()));
    THROW_IF_FAILED(stream->InitializeFromFilename(file_path.c_str(), GENERIC_WRITE));

    // 初始化编码器
    THROW_IF_FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

    // 创建新帧（带属性包用于 JPEG 质量设置）
    wil::com_ptr<IWICBitmapFrameEncode> frame;
    wil::com_ptr<IPropertyBag2> property_bag;
    THROW_IF_FAILED(encoder->CreateNewFrame(frame.put(), property_bag.put()));

    // JPEG 格式时设置质量参数
    if (format == ImageFormat::JPEG && property_bag) {
      PROPBAG2 quality_option = {};
      quality_option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
      VARIANT quality_value;
      VariantInit(&quality_value);
      quality_value.vt = VT_R4;
      quality_value.fltVal = jpeg_quality;
      property_bag->Write(1, &quality_option, &quality_value);
    }

    // 初始化帧
    THROW_IF_FAILED(frame->Initialize(property_bag.get()));

    // 设置帧尺寸
    THROW_IF_FAILED(frame->SetSize(width, height));

    // 用原始像素数据创建 WIC 位图（声明为 32bppBGRA）
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(factory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppBGRA,
                                                    row_pitch, row_pitch * height,
                                                    const_cast<BYTE*>(pixel_data), bitmap.put()));

    // 设置像素格式（让编码器协商：JPEG→24bppBGR, PNG→32bppBGRA）
    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
    THROW_IF_FAILED(frame->SetPixelFormat(&pixel_format));

    // WriteSource 自动将 32bppBGRA 转换为编码器协商的格式
    THROW_IF_FAILED(frame->WriteSource(bitmap.get(), nullptr));

    // 提交帧
    THROW_IF_FAILED(frame->Commit());

    // 提交编码器
    THROW_IF_FAILED(encoder->Commit());

    return {};
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}
}  // namespace Utils::Image