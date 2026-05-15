module;

export module Utils.Image;

import std;
import <wincodec.h>;
import <wil/com.h>;

namespace Utils::Image {
// WIC工厂类型别名
export using WICFactory = wil::com_ptr<IWICImagingFactory>;

// 线程局部存储，为每个线程维护独立的COM环境和WIC工厂
// WIC/COM 都带线程语义，所以这里不做跨线程共享。
thread_local std::optional<wil::unique_couninitialize_call> thread_com_init;
thread_local WICFactory thread_wic_factory;

// 图像信息结构
export struct ImageInfo {
  uint32_t width;
  uint32_t height;
  GUID pixel_format;
  std::string mime_type;
};

// WebP编码选项
export struct WebPEncodeOptions {
  float quality = 75.0f;  // 0-100
  bool lossless = false;
};

// WebP编码结果
export struct WebPEncodedResult {
  std::vector<uint8_t> data;
  uint32_t width;
  uint32_t height;
};

export struct BGRABitmapData {
  // 约定都用紧排 BGRA，方便在 WIC / WebP / D3D readback 之间直接传。
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t stride = 0;
  std::vector<uint8_t> pixels;
};

// 创建WIC工厂
export auto create_factory() -> std::expected<WICFactory, std::string>;

// 获取当前线程的WIC工厂。如果工厂不存在，则创建它。
export auto get_thread_wic_factory() -> std::expected<WICFactory, std::string>;

// 获取图像信息（需要传递工厂）
export auto get_image_info(IWICImagingFactory* factory, const std::filesystem::path& path)
    -> std::expected<ImageInfo, std::string>;

export auto load_scaled_bgra_bitmap_data(IWICImagingFactory* factory,
                                         const std::filesystem::path& path,
                                         uint32_t short_edge_size)
    -> std::expected<BGRABitmapData, std::string>;

export auto load_scaled_bgra_bitmap_data(IWICImagingFactory* factory, IWICBitmapSource* source,
                                         uint32_t short_edge_size)
    -> std::expected<BGRABitmapData, std::string>;

export auto encode_bgra_to_webp(const BGRABitmapData& bitmap_data,
                                const WebPEncodeOptions& options = {})
    -> std::expected<WebPEncodedResult, std::string>;

// 从内存 BGRA（如视频单帧）生成 WebP 缩略图。
export auto generate_webp_thumbnail_from_bgra(IWICImagingFactory* factory,
                                              const BGRABitmapData& bitmap_data,
                                              uint32_t short_edge_size,
                                              const WebPEncodeOptions& options = {})
    -> std::expected<WebPEncodedResult, std::string>;

// 图像输出格式
export enum class ImageFormat { PNG, JPEG };

// 将紧排 BGRA8 像素编码成 JPEG 字节流，供需要自行封装容器的调用方使用。
export auto encode_bgra_to_jpeg_bytes(IWICImagingFactory* factory, const uint8_t* pixel_data,
                                      uint32_t width, uint32_t height, uint32_t row_pitch,
                                      float jpeg_quality = 1.0f)
    -> std::expected<std::vector<uint8_t>, std::string>;

// 保存像素数据到文件
export auto save_pixel_data_to_file(IWICImagingFactory* factory, const uint8_t* pixel_data,
                                    uint32_t width, uint32_t height, uint32_t row_pitch,
                                    const std::wstring& file_path,
                                    ImageFormat format = ImageFormat::PNG,
                                    float jpeg_quality = 1.0f) -> std::expected<void, std::string>;
}  // namespace Utils::Image
