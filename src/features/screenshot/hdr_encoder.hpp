#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d3d11.hpp"

// HDR 图像编码：将捕获到的 R16G16B16A16_FLOAT 纹理写成 Ultra HDR JPEG，或无损 JPEG XR。
// 实现拆在多个 .cpp 中，本接口文件只保留调用方需要的类型与入口。

namespace features::screenshot::hdr_encoder {

struct UltraHdrEncodeOptions {
  // 底图（SDR 兼容预览）JPEG 量化质量，0–100。
  int base_quality = 100;
  // 增益图（HDR 相对 SDR 的倍数信息）JPEG 量化质量，0–100。
  int gainmap_quality = 100;
  // 目标显示器峰值亮度（nit），会写入 Ultra HDR metadata。
  // 解码端会结合这个字段判断 gain map 应该把亮部恢复到什么上限。
  float target_display_peak_nits = 1000.0f;
};

// Ultra HDR 预处理的 GPU 中间状态。直方图已经提交到 immediate context，
// 但结果暂不读回，以便调用方先完成另一条输出路径的 GPU→CPU 读回。
struct UltraHdrPreprocessSession {
  wil::com_ptr<ID3D11Device> device;
  wil::com_ptr<ID3D11DeviceContext> context;
  wil::com_ptr<ID3D11ShaderResourceView> source_srv;
  wil::com_ptr<ID3D11Buffer> histogram_buffer;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

// JXR 的 GPU 读回状态。staging texture 只在 D3D 线程上使用，Map 后即释放。
struct JxrReadbackSession {
  wil::com_ptr<ID3D11DeviceContext> context;
  wil::com_ptr<ID3D11Texture2D> staging_texture;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

// 已经脱离 D3D 资源的紧排 FP16 像素，可安全交给后台线程做 WIC 编码。
struct JxrPixelData {
  std::vector<std::uint8_t> pixels;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t row_pitch = 0;
};

struct UltraHdrPreparedImages {
  // 已经 tone-map 并做 sRGB OETF 的 SDR 兼容底图，格式为紧排 BGRA8888。
  std::vector<std::uint8_t> base_bgra8;
  // 已量化好的多通道 gain map，格式为紧排 BGRA8888。
  // GPU 侧按 BGRA 打包是为了让 D3D11/UAV/WIC 直接复用现有 4-byte 像素路径。
  std::vector<std::uint8_t> gainmap_bgra8;
  // metadata 仍然只写一套共享的 min/max 范围。
  // 这里存的是那套共享范围在 log2 域里的值，和旧 libultrahdr 的 XMP/ISO 语义一致。
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

// 以下函数供本模块的不同实现单元复用，原型集中放在接口单元，避免实现文件里手写前向声明。
// 提交 Ultra HDR 直方图 GPU 任务，但暂不阻塞读取结果。
auto begin_ultrahdr_preprocess(ID3D11Texture2D* texture)
    -> std::expected<UltraHdrPreprocessSession, std::string>;
// 读取直方图并继续完成剩余 GPU 预处理和输出图层读回。
auto finish_ultrahdr_preprocess(UltraHdrPreprocessSession& session)
    -> std::expected<UltraHdrPreparedImages, std::string>;
// 预处理：GPU 先生成 SDR base 和 HDR half，再完成 gain 计算、tile 范围归约与量化。
// CPU 只对归约后的全局 min/max 做 clamp 与最小范围展开。
auto preprocess_texture_for_ultrahdr(ID3D11Texture2D* texture)
    -> std::expected<UltraHdrPreparedImages, std::string>;
// 从 GPU 统计出的 gain 范围和用户目标显示峰值构造最终 metadata 语义。
auto build_gainmap_metadata(const UltraHdrPreparedImages& images,
                            const UltraHdrEncodeOptions& options) -> GainMapMetadata;
// Ultra HDR JPEG 封装：用 WIC 编出两张 JPEG，再由项目内代码直接写 XMP/ISO/MPF 容器。
auto encode_ultrahdr_jpeg(const UltraHdrPreparedImages& images,
                          const UltraHdrEncodeOptions& options)
    -> std::expected<std::vector<std::uint8_t>, std::string>;
auto write_file(const std::wstring& file_path, const std::vector<std::uint8_t>& data)
    -> std::expected<void, std::string>;

// 将已经完成 GPU 预处理的图层编码并写入 Ultra HDR JPEG。
auto save_prepared_images_as_ultrahdr_jpeg(const UltraHdrPreparedImages& images,
                                           const std::wstring& file_path,
                                           const UltraHdrEncodeOptions& options = {})
    -> std::expected<void, std::string>;

// 生成 SDR 底图 + gain map → 编码为 Ultra HDR JPEG 并写入 path。
auto save_texture_as_ultrahdr_jpeg(ID3D11Texture2D* texture, const std::wstring& file_path,
                                   const UltraHdrEncodeOptions& options = {})
    -> std::expected<void, std::string>;

// 把 HDR 纹理复制到 staging texture；调用方可在 GPU 继续处理期间延后 Map。
auto begin_jxr_readback(ID3D11Texture2D* texture) -> std::expected<JxrReadbackSession, std::string>;
// 在 D3D 线程上完成 Map，并复制成不依赖 staging texture 的紧排像素。
auto read_jxr_pixels(JxrReadbackSession session) -> std::expected<JxrPixelData, std::string>;
// 在不接触 D3D 的线程上用 WIC 编码并保存 JXR。
auto save_jxr_pixels(const JxrPixelData& pixels, const std::wstring& file_path)
    -> std::expected<void, std::string>;

}  // namespace features::screenshot::hdr_encoder
