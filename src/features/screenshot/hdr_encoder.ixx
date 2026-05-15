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
  // 目标显示器峰值亮度（nit），会写入 Ultra HDR metadata。
  // 解码端会结合这个字段判断 gain map 应该把亮部恢复到什么上限。
  float target_display_peak_nits = 1000.0f;
};

struct UltraHdrPreparedImages {
  // 已经 tone-map 并做 sRGB OETF 的 SDR 兼容底图，格式为紧排 BGRA8888。
  std::vector<std::uint8_t> base_bgra8;
  // 已量化好的单通道 gain map，格式为紧排 Gray8。
  std::vector<std::uint8_t> gainmap_gray8;
  // 量化 gain map 前的内容范围。metadata 与量化都必须基于同一对数值。
  // 这里存 log2 域，是因为 Ultra HDR 语义本身就是“相对 SDR 的倍数”。
  float min_gain_log2 = 0.0f;
  float max_gain_log2 = 0.0f;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

struct GainMapMetadata {
  // 这些字段对应的是“浮点语义模型”。真正写进 JPEG 前，
  // 还会被转换成 ISO 21496-1 需要的分数字段。
  float min_content_boost = 1.0f;
  float max_content_boost = 1.0f;
  float gamma = 1.0f;
  float offset_sdr = 1e-7f;
  float offset_hdr = 1e-7f;
  float hdr_capacity_min = 1.0f;
  float hdr_capacity_max = 1.0f;
  bool use_base_cg = true;
};

// 以下函数仅在本模块各 .cpp 实现单元之间可见（未 export），外部 `import` 本模块时无法使用。
// 原型集中放在接口单元，避免实现文件里手写前向声明。
// GPU 预处理：从 WGC 的 R16G16B16A16_FLOAT 纹理生成 SDR base 和量化后的 gain map。
auto preprocess_texture_for_ultrahdr(ID3D11Texture2D* texture)
    -> std::expected<UltraHdrPreparedImages, std::string>;
// 从 GPU 统计出的 gain 范围和用户目标显示峰值构造最终 metadata 语义。
auto build_gainmap_metadata(const UltraHdrPreparedImages& images,
                            const UltraHdrEncodeOptions& options) -> GainMapMetadata;
// JPEG_R 封装：用 WIC 编出两张 JPEG，再由项目内代码直接写 XMP/ISO/MPF 容器。
auto encode_ultrahdr_jpeg(const UltraHdrPreparedImages& images,
                          const UltraHdrEncodeOptions& options)
    -> std::expected<std::vector<std::uint8_t>, std::string>;
auto write_file(const std::wstring& file_path, const std::vector<std::uint8_t>& data)
    -> std::expected<void, std::string>;

// GPU 生成 SDR 底图 + gain map → 编码为 Ultra HDR JPEG 并写入 path。
export auto save_texture_as_ultrahdr_jpeg(ID3D11Texture2D* texture, const std::wstring& file_path,
                                          const UltraHdrEncodeOptions& options = {})
    -> std::expected<void, std::string>;

}  // namespace Features::Screenshot::HdrEncoder
