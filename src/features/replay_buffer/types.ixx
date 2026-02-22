module;

export module Features.ReplayBuffer.Types;

import std;

namespace Features::ReplayBuffer::Types {

// 回放缓冲状态
export enum class ReplayBufferStatus {
  Idle,       // 空闲
  Buffering,  // 正在缓冲
  Saving,     // 正在保存
  Error       // 发生错误
};

// 回放缓冲配置（运行时参数）
export struct ReplayBufferConfig {
  // 后台录制参数（来自 recording 或 motion_photo）
  std::uint32_t fps = 30;               // 帧率
  std::uint32_t bitrate = 20'000'000;   // 视频比特率
  std::uint32_t quality = 70;           // 质量值 (0-100, VBR 模式)
  std::string codec = "h264";           // 视频编码格式
  std::string rate_control = "cbr";     // 码率控制模式
  std::string encoder_mode = "auto";    // 编码器模式
  std::uint32_t keyframe_interval = 1;  // 关键帧间隔（秒），裁剪精度

  // 捕获配置
  bool use_recording_capture_options = false;  // 是否启用录制继承的捕获参数
  bool capture_client_area = false;            // 是否只捕获客户区
  bool capture_cursor = false;                 // 是否捕获鼠标指针

  // 音频配置
  std::string audio_source = "system";    // 音频源: "none" | "system" | "game_only"
  std::uint32_t audio_bitrate = 256'000;  // 音频码率

  // 即时回放参数
  std::uint32_t max_duration = 30;  // 即时回放最大时长（秒）

  // Motion Photo 参数
  std::uint32_t motion_photo_duration = 3;       // Motion Photo 视频时长（秒）
  std::uint32_t motion_photo_resolution = 1080;  // Motion Photo 目标短边分辨率，0 表示不缩放
};

}  // namespace Features::ReplayBuffer::Types
