module;

#include <d3d11.h>
#include <windows.h>

export module Features.Screenshot.HdrEncoder;

import std;

// Ultra HDR（JPEG_R）编码：将捕获到的 R16G16B16A16_FLOAT 纹理写成带增益图的 JPEG。
// 实现拆在多个 .cpp 中，本接口文件只保留调用方需要的类型与入口。

namespace Features::Screenshot::HdrEncoder {

export struct UltraHdrEncodeOptions {
  // 底图（SDR 兼容预览）JPEG 量化质量，0–100。
  int base_quality = 100;
  // 增益图（HDR 相对 SDR 的倍数信息）JPEG 量化质量，0–100。
  int gainmap_quality = 100;
  // 告诉 libultrahdr 目标显示器峰值亮度（nit），会写入元数据并影响色调映射侧假设。
  float target_display_peak_nits = 1000.0f;
};

struct UltraHdrPreparedImages {
  // 已经 tone-map 并做 sRGB OETF 的 SDR 兼容底图，格式为紧排 RGBA8888。
  std::vector<std::uint8_t> sdr_pixels;
  // 已按 80 nit -> 203 nit 参考白缩放后的 HDR 图层，格式为紧排 RGBA half。
  std::vector<std::uint16_t> hdr_pixels;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

// 以下函数仅在本模块各 .cpp 实现单元之间可见（未 export），外部 `import` 本模块时无法使用。
// 原型集中放在接口单元，避免实现文件里手写前向声明。
// GPU 预处理：从 WGC 的 R16G16B16A16_FLOAT 纹理生成 libultrahdr raw image 所需的两块 CPU 内存。
auto preprocess_texture_for_ultrahdr(ID3D11Texture2D* texture)
    -> std::expected<UltraHdrPreparedImages, std::string>;
// JPEG_R 封装：只负责把预处理好的 SDR/HDR raw image 交给 libultrahdr，并做 APP 顺序修正。
auto encode_ultrahdr_jpeg(const UltraHdrPreparedImages& images,
                          const UltraHdrEncodeOptions& options)
    -> std::expected<std::vector<std::uint8_t>, std::string>;
auto write_file(const std::wstring& file_path, const std::vector<std::uint8_t>& data)
    -> std::expected<void, std::string>;

// GPU 生成 SDR 底图 + HDR 半精度平面 → 编码为 Ultra HDR JPEG 并写入 path。
export auto save_texture_as_ultrahdr_jpeg(ID3D11Texture2D* texture, const std::wstring& file_path,
                                          const UltraHdrEncodeOptions& options = {})
    -> std::expected<void, std::string>;

}  // namespace Features::Screenshot::HdrEncoder
