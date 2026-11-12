#include "features/recording/audio_negotiation.hpp"

namespace features::recording::audio_negotiation {

auto target_bytes_per_second(std::uint32_t bitrate_bps) -> std::uint64_t {
  return (std::uint64_t(bitrate_bps) + 7) / 8;
}

auto select_candidate(const std::vector<OutputCandidate>& candidates,
                      std::uint32_t target_bitrate_bps, std::uint32_t sample_rate,
                      std::uint32_t channels) -> std::optional<std::size_t> {
  const auto target = target_bytes_per_second(target_bitrate_bps);

  std::optional<std::size_t> lowest_meeting;
  std::optional<std::size_t> highest_below;
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const auto& candidate = candidates[index];
    // 码率是协商必需项；采样率/声道为 0 表示编码器未声明，不据此排除候选
    if (!candidate.bytes_per_second ||
        (candidate.sample_rate && candidate.sample_rate != sample_rate) ||
        (candidate.channels && candidate.channels != channels)) {
      continue;
    }
    if (candidate.bytes_per_second >= target) {
      if (!lowest_meeting ||
          candidate.bytes_per_second < candidates[*lowest_meeting].bytes_per_second) {
        lowest_meeting = index;
      }
    } else if (!highest_below ||
               candidate.bytes_per_second > candidates[*highest_below].bytes_per_second) {
      highest_below = index;
    }
  }

  return lowest_meeting ? lowest_meeting : highest_below;
}

}  // namespace features::recording::audio_negotiation
