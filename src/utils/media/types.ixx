module;

export module Utils.Media.Encoder.Types;

import std;

namespace Utils::Media::Encoder::Types {

// 码率控制模式
export enum class RateControlMode {
  CBR,      // 固定码率 - 使用 bitrate
  VBR,      // 质量优先 VBR - 使用 quality (0-100)
  ManualQP  // 手动 QP 模式 - 使用 qp (0-51)
};

// 从字符串转换为 RateControlMode
export constexpr RateControlMode rate_control_mode_from_string(std::string_view str) {
  if (str == "vbr") return RateControlMode::VBR;
  if (str == "manual_qp") return RateControlMode::ManualQP;
  return RateControlMode::CBR;  // 默认
}

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

// 音频源类型
export enum class AudioSource {
  None,     // 不录制音频
  System,   // 系统全部音频（传统 Loopback）
  GameOnly  // 仅游戏音频（Process Loopback，需 Windows 10 2004+）
};

// 从字符串转换为 AudioSource
export constexpr AudioSource audio_source_from_string(std::string_view str) {
  if (str == "none") return AudioSource::None;
  if (str == "game_only") return AudioSource::GameOnly;
  return AudioSource::System;  // 默认
}

// 编码器配置
export struct EncoderConfig {
  std::filesystem::path output_path;                    // 输出文件路径
  std::uint32_t width = 0;                              // 视频宽度
  std::uint32_t height = 0;                             // 视频高度
  std::uint32_t fps = 30;                               // 帧率
  std::uint32_t bitrate = 80'000'000;                   // 视频比特率 (默认 80Mbps, CBR 模式使用)
  std::uint32_t quality = 70;                           // 质量值 (0-100, VBR 模式使用)
  std::uint32_t qp = 23;                                // 量化参数 (0-51, ManualQP 模式使用)
  std::uint32_t keyframe_interval = 2;                  // 关键帧间隔（秒），默认 2s
  RateControlMode rate_control = RateControlMode::CBR;  // 码率控制模式
  EncoderMode encoder_mode = EncoderMode::Auto;         // 编码器模式
  VideoCodec codec = VideoCodec::H264;                  // 视频编码格式 (默认 H.264)

  // 音频配置
  std::uint32_t audio_bitrate = 256'000;  // 音频码率 (默认 256kbps)
};

}  // namespace Utils::Media::Encoder::Types
