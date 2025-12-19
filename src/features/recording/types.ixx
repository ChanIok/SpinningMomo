module;

export module Features.Recording.Types;

import std;

namespace Features::Recording::Types {

// 编码器模式
export enum class EncoderMode {
  Auto,  // 自动检测，优先 GPU
  GPU,   // 强制 GPU（不可用则失败）
  CPU    // 强制 CPU
};

// 从字符串转换为 EncoderMode
export constexpr EncoderMode encoder_mode_from_string(std::string_view str) {
  if (str == "gpu") return EncoderMode::GPU;
  if (str == "cpu") return EncoderMode::CPU;
  return EncoderMode::Auto;  // 默认或 "auto"
}

// 视频编码格式
export enum class VideoCodec {
  H264,  // H.264/AVC
  H265   // H.265/HEVC
};

// 从字符串转换为 VideoCodec
export constexpr VideoCodec video_codec_from_string(std::string_view str) {
  if (str == "h265" || str == "hevc") return VideoCodec::H265;
  return VideoCodec::H264;  // 默认
}

// 录制配置
export struct RecordingConfig {
  std::filesystem::path output_path;             // 输出文件路径
  std::uint32_t width = 0;                       // 视频宽度
  std::uint32_t height = 0;                      // 视频高度
  std::uint32_t fps = 30;                        // 帧率
  std::uint32_t bitrate = 80'000'000;            // 比特率 (默认 80Mbps)
  EncoderMode encoder_mode = EncoderMode::Auto;  // 编码器模式
  VideoCodec codec = VideoCodec::H264;           // 视频编码格式 (默认 H.264)
};

// 录制状态枚举
export enum class RecordingStatus {
  Idle,       // 空闲
  Recording,  // 正在录制
  Stopping,   // 正在停止
  Error       // 发生错误
};

}  // namespace Features::Recording::Types
