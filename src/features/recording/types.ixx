module;

#include <filesystem>

export module Features.Recording.Types;

import std;

export namespace Features::Recording::Types {

// 编码器模式
enum class EncoderMode {
  Auto,  // 自动检测，优先 GPU
  GPU,   // 强制 GPU（不可用则失败）
  CPU    // 强制 CPU
};

// 录制配置
struct RecordingConfig {
  std::filesystem::path output_path;  // 输出文件路径
  uint32_t width = 0;                 // 视频宽度
  uint32_t height = 0;                // 视频高度
  uint32_t fps = 30;                  // 帧率
  uint32_t bitrate = 80'000'000;       // 比特率 (默认 80Mbps)
  EncoderMode encoder_mode = EncoderMode::Auto;  // 编码器模式
};

// 录制状态枚举
enum class RecordingStatus {
  Idle,      // 空闲
  Recording, // 正在录制
  Stopping,  // 正在停止
  Error      // 发生错误
};

}  // namespace Features::Recording::Types
