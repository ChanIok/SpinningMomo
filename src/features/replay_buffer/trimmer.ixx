module;

export module Features.ReplayBuffer.Trimmer;

import std;

namespace Features::ReplayBuffer::Trimmer {

// 缩放配置（可选）
export struct ScaleConfig {
  std::uint32_t target_short_edge = 0;  // 目标短边分辨率，0 表示不缩放
  std::uint32_t bitrate = 10'000'000;   // 输出码率
  std::uint32_t fps = 30;               // 输出帧率
};

// 从多个 MP4 段落中裁剪/拼接出指定时长的视频
// input_paths: 按时间顺序排列的输入文件
// output_path: 输出文件路径
// duration_seconds: 从末尾截取的目标时长（秒）
// scale_config: 缩放配置，空表示不缩放（Stream Copy 模式）
export auto trim_and_concat(const std::vector<std::filesystem::path>& input_paths,
                            const std::filesystem::path& output_path, double duration_seconds,
                            std::optional<ScaleConfig> scale_config = std::nullopt)
    -> std::expected<void, std::string>;

}  // namespace Features::ReplayBuffer::Trimmer
