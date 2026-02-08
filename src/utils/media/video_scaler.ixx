module;

#include <d3d11.h>
#include <mfidl.h>
#include <wil/com.h>

export module Utils.Media.VideoScaler;

import std;

export namespace Utils::Media::VideoScaler {

// 视频缩放配置
struct ScaleConfig {
  std::uint32_t target_width = 0;       // 目标宽度（0 表示自动计算）
  std::uint32_t target_height = 0;      // 目标高度（0 表示自动计算）
  std::uint32_t target_short_edge = 0;  // 目标短边（优先级高于 width/height）
  std::uint32_t bitrate = 8'000'000;    // 视频码率
  std::uint32_t fps = 30;               // 帧率
};

// 缩放结果
struct ScaleResult {
  bool scaled;                  // 是否实际进行了缩放（false 表示源分辨率已符合目标）
  std::uint32_t src_width;      // 源宽度
  std::uint32_t src_height;     // 源高度
  std::uint32_t target_width;   // 目标宽度
  std::uint32_t target_height;  // 目标高度
};

// 使用 D3D11 Video Processor 进行硬件加速视频缩放
// 完整流程：SourceReader(D3D11) → VideoProcessor(缩放) → SinkWriter(D3D11硬件编码)
auto scale_video_file(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path, const ScaleConfig& config)
    -> std::expected<ScaleResult, std::string>;

}  // namespace Utils::Media::VideoScaler
