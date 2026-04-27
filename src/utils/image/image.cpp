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

auto copy_bgra_source_pixels(IWICBitmapSource* source)
    -> std::expected<BGRABitmapData, std::string> {
  if (!source) {
    return std::unexpected("Source bitmap is null");
  }

  try {
    UINT width = 0;
    UINT height = 0;
    THROW_IF_FAILED(source->GetSize(&width, &height));

    if (width == 0 || height == 0) {
      return std::unexpected("Bitmap source has empty dimensions");
    }

    if (width > static_cast<UINT>(std::numeric_limits<INT>::max()) ||
        height > static_cast<UINT>(std::numeric_limits<INT>::max())) {
      return std::unexpected("Bitmap source is too large to copy");
    }

    // 这里约定输出总是紧密排列的 32bpp BGRA，方便后续直接交给 libwebp。
    constexpr std::uint32_t kBgraBytesPerPixel = 4;
    auto stride_bytes = static_cast<std::uint64_t>(width) * kBgraBytesPerPixel;
    auto buffer_size = stride_bytes * height;
    if (stride_bytes > std::numeric_limits<UINT>::max() ||
        buffer_size > std::numeric_limits<UINT>::max()) {
      return std::unexpected("Bitmap source buffer is too large to copy");
    }

    BGRABitmapData result;
    result.width = static_cast<std::uint32_t>(width);
    result.height = static_cast<std::uint32_t>(height);
    result.stride = static_cast<std::uint32_t>(stride_bytes);
    result.pixels.resize(static_cast<std::size_t>(buffer_size));

    // CopyPixels 是这条流水线真正落到内存的地方；前面的 WIC source 仍可以保持惰性。
    WICRect rect = {0, 0, static_cast<INT>(width), static_cast<INT>(height)};
    THROW_IF_FAILED(source->CopyPixels(&rect, result.stride, static_cast<UINT>(buffer_size),
                                       result.pixels.data()));

    return result;
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto convert_source_to_bgra_data(IWICImagingFactory* factory, IWICBitmapSource* source)
    -> std::expected<BGRABitmapData, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!source) {
    return std::unexpected("Source bitmap is null");
  }

  try {
    // WIC 解码器输出的像素格式不固定，统一转成 BGRA 后再拷贝。
    wil::com_ptr<IWICFormatConverter> converter;
    THROW_IF_FAILED(factory->CreateFormatConverter(converter.put()));
    THROW_IF_FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppBGRA,
                                          WICBitmapDitherTypeNone, nullptr, 0.0,
                                          WICBitmapPaletteTypeCustom));

    return copy_bgra_source_pixels(converter.get());
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto load_scaled_bgra_bitmap_data(IWICImagingFactory* factory, IWICBitmapSource* source,
                                  uint32_t short_edge_size)
    -> std::expected<BGRABitmapData, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (!source) {
    return std::unexpected("Source bitmap is null");
  }

  try {
    UINT original_width = 0;
    UINT original_height = 0;
    THROW_IF_FAILED(source->GetSize(&original_width, &original_height));

    auto [new_width, new_height] =
        calculate_scaled_size(original_width, original_height, short_edge_size);

    // 保持 frame -> scaler 紧邻。WIC 在 JPEG 等格式上可借此使用解码期降采样。
    IWICBitmapSource* scaled_source = source;
    wil::com_ptr<IWICBitmapScaler> scaler;
    if (new_width != original_width || new_height != original_height) {
      THROW_IF_FAILED(factory->CreateBitmapScaler(scaler.put()));
      THROW_IF_FAILED(
          scaler->Initialize(source, new_width, new_height, WICBitmapInterpolationModeFant));
      scaled_source = scaler.get();
    }

    return convert_source_to_bgra_data(factory, scaled_source);
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto load_scaled_bgra_bitmap_data(IWICImagingFactory* factory, const std::filesystem::path& path,
                                  uint32_t short_edge_size)
    -> std::expected<BGRABitmapData, std::string> {
  // 文件入口只负责打开第一帧，真正的缩放/格式转换复用 source 入口。
  auto frame_result = load_bitmap_frame(factory, path);
  if (!frame_result) {
    return std::unexpected(frame_result.error());
  }

  return load_scaled_bgra_bitmap_data(factory, frame_result->get(), short_edge_size);
}

auto scale_bgra_bitmap_data(IWICImagingFactory* factory, const BGRABitmapData& bitmap_data,
                            uint32_t short_edge_size)
    -> std::expected<BGRABitmapData, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (bitmap_data.width == 0 || bitmap_data.height == 0 || bitmap_data.stride == 0) {
    return std::unexpected("Bitmap data is empty");
  }

  if (bitmap_data.pixels.empty()) {
    return std::unexpected("Bitmap pixels are empty");
  }

  auto minimum_stride =
      static_cast<std::uint64_t>(bitmap_data.width) * static_cast<std::uint64_t>(4);
  if (bitmap_data.stride < minimum_stride) {
    return std::unexpected("Bitmap stride is smaller than width * 4");
  }

  auto required_bytes = static_cast<std::uint64_t>(bitmap_data.stride) *
                        static_cast<std::uint64_t>(bitmap_data.height);
  if (required_bytes > bitmap_data.pixels.size()) {
    return std::unexpected("Bitmap pixel buffer is smaller than stride * height");
  }

  if (bitmap_data.pixels.size() > std::numeric_limits<UINT>::max()) {
    return std::unexpected("Bitmap data is too large for WIC");
  }

  try {
    // 视频封面等来源已经是内存 BGRA，先包成 WIC source，再复用同一套缩放逻辑。
    wil::com_ptr<IWICBitmap> bitmap;
    THROW_IF_FAILED(factory->CreateBitmapFromMemory(
        bitmap_data.width, bitmap_data.height, GUID_WICPixelFormat32bppBGRA, bitmap_data.stride,
        static_cast<UINT>(bitmap_data.pixels.size()), const_cast<BYTE*>(bitmap_data.pixels.data()),
        bitmap.put()));

    return load_scaled_bgra_bitmap_data(factory, bitmap.get(), short_edge_size);
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

auto encode_bgra_to_webp(const BGRABitmapData& bitmap_data, const WebPEncodeOptions& options)
    -> std::expected<WebPEncodedResult, std::string> {
  if (bitmap_data.width == 0 || bitmap_data.height == 0 || bitmap_data.stride == 0) {
    return std::unexpected("Bitmap data is empty");
  }

  if (bitmap_data.pixels.empty()) {
    return std::unexpected("Bitmap pixels are empty");
  }

  if (bitmap_data.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      bitmap_data.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      bitmap_data.stride > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    return std::unexpected("Bitmap data dimensions are too large for WebP");
  }

  auto minimum_stride =
      static_cast<std::uint64_t>(bitmap_data.width) * static_cast<std::uint64_t>(4);
  if (bitmap_data.stride < minimum_stride) {
    return std::unexpected("Bitmap stride is smaller than width * 4");
  }

  auto required_bytes = static_cast<std::uint64_t>(bitmap_data.stride) *
                        static_cast<std::uint64_t>(bitmap_data.height);
  if (required_bytes > bitmap_data.pixels.size()) {
    return std::unexpected("Bitmap pixel buffer is smaller than stride * height");
  }

  try {
    // libwebp 只需要 BGRA 指针、宽高和 stride；不再额外创建 WIC bitmap。
    uint8_t* output = nullptr;
    size_t output_size = 0;
    auto width = static_cast<int>(bitmap_data.width);
    auto height = static_cast<int>(bitmap_data.height);
    auto stride = static_cast<int>(bitmap_data.stride);

    if (options.lossless) {
      output_size =
          WebPEncodeLosslessBGRA(bitmap_data.pixels.data(), width, height, stride, &output);
    } else {
      output_size = WebPEncodeBGRA(bitmap_data.pixels.data(), width, height, stride,
                                   options.quality, &output);
    }

    if (output_size == 0 || output == nullptr) {
      return std::unexpected("Failed to encode WebP image");
    }

    // libwebp 分配输出内存；复制到 vector 后必须用 WebPFree 释放。
    std::vector<uint8_t> result_data(output, output + output_size);
    WebPFree(output);

    return WebPEncodedResult{std::move(result_data), bitmap_data.width, bitmap_data.height};
  } catch (const wil::ResultException& e) {
    return std::unexpected(format_hresult(e.GetErrorCode(), "WIC operation failed"));
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception: ") + e.what());
  }
}

// 视频封面：MF 已解码为 RGB32/BGRA 内存帧，无需落盘即可走与照片相同的缩放 + WebP 编码。
auto generate_webp_thumbnail_from_bgra(IWICImagingFactory* factory,
                                       const BGRABitmapData& bitmap_data, uint32_t short_edge_size,
                                       const WebPEncodeOptions& options)
    -> std::expected<WebPEncodedResult, std::string> {
  if (!factory) {
    return std::unexpected("WIC factory is null");
  }

  if (bitmap_data.width == 0 || bitmap_data.height == 0 || bitmap_data.stride == 0) {
    return std::unexpected("Bitmap data is empty");
  }

  if (bitmap_data.pixels.empty()) {
    return std::unexpected("Bitmap pixels are empty");
  }

  auto scaled_result = scale_bgra_bitmap_data(factory, bitmap_data, short_edge_size);
  if (!scaled_result) {
    return std::unexpected(scaled_result.error());
  }

  return encode_bgra_to_webp(scaled_result.value(), options);
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
