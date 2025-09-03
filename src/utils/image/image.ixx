module;
#include <wincodec.h>
#include <wil/com.h>

export module Utils.Image;

import std;

namespace Utils::Image {
    // WIC工厂类型别名
    export using WICFactory = wil::com_ptr<IWICImagingFactory>;
    
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
    
    // 获取图像信息（需要传递工厂）
    export auto get_image_info(IWICImagingFactory* factory, const std::filesystem::path& path)
        -> std::expected<ImageInfo, std::string>;
    
    // 加载图像帧（供其他模块使用）
    export auto load_bitmap_frame(IWICImagingFactory* factory, const std::filesystem::path& path)
        -> std::expected<wil::com_ptr<IWICBitmapFrameDecode>, std::string>;
    
    // 转换为WIC位图
    export auto convert_to_bitmap(IWICImagingFactory* factory, IWICBitmapSource* source)
        -> std::expected<wil::com_ptr<IWICBitmap>, std::string>;
    
    // 缩放图像
    export auto scale_bitmap(IWICImagingFactory* factory, IWICBitmapSource* source,
                           uint32_t max_width, uint32_t max_height)
        -> std::expected<wil::com_ptr<IWICBitmap>, std::string>;
    
    // 将WIC位图编码为WebP
    export auto encode_bitmap_to_webp(IWICBitmap* bitmap, const WebPEncodeOptions& options = {})
        -> std::expected<WebPEncodedResult, std::string>;
    
    // 直接从文件生成WebP缩略图
    export auto generate_webp_thumbnail(WICFactory& factory,
                                      const std::filesystem::path& path,
                                      uint32_t max_width, uint32_t max_height,
                                      const WebPEncodeOptions& options = {})
        -> std::expected<WebPEncodedResult, std::string>;
    
    // 保存像素数据到文件
    export auto save_pixel_data_to_file(IWICImagingFactory* factory,
                                      const uint8_t* pixel_data,
                                      uint32_t width,
                                      uint32_t height,
                                      uint32_t row_pitch,
                                      const std::wstring& file_path)
        -> std::expected<void, std::string>;
}