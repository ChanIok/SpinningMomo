#pragma once

#include "vendor/std.hpp"

namespace features::recording::audio_negotiation {

// AAC 编码器通过 GetOutputAvailableType 报出的候选输出类型摘要
struct OutputCandidate {
  std::uint32_t bytes_per_second = 0;  // MF_MT_AUDIO_AVG_BYTES_PER_SECOND，必需项
  std::uint32_t sample_rate = 0;       // MF_MT_AUDIO_SAMPLES_PER_SECOND，0 表示编码器未声明
  std::uint32_t channels = 0;          // MF_MT_AUDIO_NUM_CHANNELS，0 表示编码器未声明
};

// 目标码率（bps）换算为输出类型使用的平均字节率（B/s）
auto target_bytes_per_second(std::uint32_t bitrate_bps) -> std::uint64_t;

// 过滤与输入格式不符的候选，返回逐个尝试 SetOutputType 的顺序：
// 过滤与输入格式不符的候选，只选一个码率档位：优先选择不低于目标的最低档，
// 若目标超过所有档位则选择最高档；码率相同时保持编码器枚举顺序。
auto select_candidate(const std::vector<OutputCandidate>& candidates,
                      std::uint32_t target_bitrate_bps, std::uint32_t sample_rate,
                      std::uint32_t channels) -> std::optional<std::size_t>;

}  // namespace features::recording::audio_negotiation
