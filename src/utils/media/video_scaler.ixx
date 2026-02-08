module;

#include <d3d11.h>
#include <mfidl.h>
#include <wil/com.h>

export module Utils.Media.VideoScaler;

import std;

export namespace Utils::Media::VideoScaler {

// 码率控制模式
enum class RateControl {
  CBR,  // 固定码率
  VBR,  // 可变码率（质量优先）
};

// 视频编码格式
enum class VideoCodec {
  H264,  // H.264/AVC
  H265,  // H.265/HEVC
};

// 视频缩放配置
struct ScaleConfig {
  std::uint32_t target_width = 0;               // 目标宽度（0 表示自动计算）
  std::uint32_t target_height = 0;              // 目标高度（0 表示自动计算）
  std::uint32_t target_short_edge = 0;          // 目标短边（0=不缩放，720/1080/1440/2160）
  std::uint32_t bitrate = 8'000'000;            // 视频码率
  std::uint32_t fps = 30;                       // 帧率
  RateControl rate_control = RateControl::VBR;  // 码率控制模式，默认 VBR
  std::uint32_t quality = 100;                  // VBR 质量（0-100），仅 VBR 模式有效
  VideoCodec codec = VideoCodec::H264;          // 视频编码格式
  std::uint32_t audio_bitrate = 192'000;        // 音频码率 (bps)，AAC 编码
};

// 缩放结果
struct ScaleResult {
  bool scaled;                  // 是否实际进行了缩放（false 表示源分辨率已符合目标）
  std::uint32_t src_width;      // 源宽度
  std::uint32_t src_height;     // 源高度
  std::uint32_t target_width;   // 目标宽度
  std::uint32_t target_height;  // 目标高度
};

// 使用 Media Foundation Transcode API 进行硬件加速视频缩放
// 完整流程：IMFMediaSource → MFCreateTranscodeTopology → IMFMediaSession
auto scale_video_file(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path, const ScaleConfig& config)
    -> std::expected<ScaleResult, std::string>;

}  // namespace Utils::Media::VideoScaler
