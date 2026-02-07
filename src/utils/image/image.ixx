module;

export module Utils.Image;

import std;
import <wincodec.h>;
import <wil/com.h>;

namespace Utils::Image {
// WIC工厂类型别名
export using WICFactory = wil::com_ptr<IWICImagingFactory>;

// 线程局部存储，为每个线程维护独立的COM环境和WIC工厂
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

// 创建WIC工厂
export auto create_factory() -> std::expected<WICFactory, std::string>;

// 获取当前线程的WIC工厂。如果工厂不存在，则创建它。
export auto get_thread_wic_factory() -> std::expected<WICFactory, std::string>;

// 获取图像信息（需要传递工厂）
export auto get_image_info(IWICImagingFactory* factory, const std::filesystem::path& path)
    -> std::expected<ImageInfo, std::string>;

// 直接从文件生成WebP缩略图（按短边等比例缩放）
export auto generate_webp_thumbnail(WICFactory& factory, const std::filesystem::path& path,
                                    uint32_t short_edge_size, const WebPEncodeOptions& options = {})
    -> std::expected<WebPEncodedResult, std::string>;

// 图像输出格式
export enum class ImageFormat { PNG, JPEG };

// 保存像素数据到文件
export auto save_pixel_data_to_file(IWICImagingFactory* factory, const uint8_t* pixel_data,
                                    uint32_t width, uint32_t height, uint32_t row_pitch,
                                    const std::wstring& file_path,
                                    ImageFormat format = ImageFormat::PNG,
                                    float jpeg_quality = 1.0f) -> std::expected<void, std::string>;
}  // namespace Utils::Image
