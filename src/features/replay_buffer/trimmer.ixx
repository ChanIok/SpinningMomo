module;

export module Features.ReplayBuffer.Trimmer;

import std;

namespace Features::ReplayBuffer::Trimmer {

// 缩放配置
export struct ScaleConfig {
  std::uint32_t target_short_edge;  // 目标短边分辨率
  std::uint32_t bitrate;            // 视频码率
  std::uint32_t fps;                // 帧率
};

// 视频缩放（Motion Photo 专用）
// 将输入视频缩放到目标短边分辨率，重新编码为 H.264 + AAC
// 若源短边 <= target_short_edge，保持原分辨率转码
export auto scale_video(const std::filesystem::path& input_path,
                        const std::filesystem::path& output_path, const ScaleConfig& config)
    -> std::expected<void, std::string>;

}  // namespace Features::ReplayBuffer::Trimmer
