#include "vendor/std.hpp"

#include "vendor/doctest.hpp"

#include "features/recording/audio_negotiation.hpp"

using features::recording::audio_negotiation::OutputCandidate;
using features::recording::audio_negotiation::select_candidate;
using features::recording::audio_negotiation::target_bytes_per_second;

namespace {

constexpr std::uint32_t sample_rate = 48'000;
constexpr std::uint32_t channels = 2;

auto selection_for(const std::vector<std::uint32_t>& bytes, std::uint32_t target_bps,
                   std::uint32_t rate = sample_rate, std::uint32_t ch = channels)
    -> std::optional<std::size_t> {
  std::vector<OutputCandidate> candidates;
  for (const auto value : bytes) {
    candidates.push_back({.bytes_per_second = value, .sample_rate = rate, .channels = ch});
  }
  return select_candidate(candidates, target_bps, sample_rate, channels);
}

}  // namespace

// 配置值原样进入协商，不做上下限夹取
TEST_CASE("target bytes per second converts bps without overflow") {
  CHECK(target_bytes_per_second(96'000) == 12'000ULL);
  CHECK(target_bytes_per_second(192'000) == 24'000ULL);
  CHECK(target_bytes_per_second(192'001) == 24'001ULL);
  CHECK(target_bytes_per_second(std::numeric_limits<std::uint32_t>::max()) == 536'870'912ULL);
}

// 码率是期望下限：向上取到能实现它的最低档，不向下就近
TEST_CASE("bitrate is raised to the lowest achieving tier") {
  const auto supported = std::vector<std::uint32_t>{12'000, 16'000, 20'000, 24'000};
  CHECK(selection_for(supported, 0) == 0U);
  CHECK(selection_for(supported, 64'000) == 0U);
  CHECK(selection_for(supported, 96'000) == 0U);
  CHECK(selection_for(supported, 100'000) == 1U);
  CHECK(selection_for(supported, 128'000) == 1U);
  CHECK(selection_for(supported, 144'000) == 2U);
  CHECK(selection_for(supported, 160'000) == 2U);
  CHECK(selection_for(supported, 192'000) == 3U);
}

TEST_CASE("targets beyond the highest tier saturate") {
  const auto supported = std::vector<std::uint32_t>{12'000, 16'000, 20'000, 24'000};
  CHECK(selection_for(supported, 250'000) == 3U);
  CHECK(selection_for(supported, 192'000'000) == 3U);
  CHECK(selection_for(supported, std::numeric_limits<std::uint32_t>::max()) == 3U);
}

TEST_CASE("formats that do not match the PCM input are excluded") {
  CHECK_FALSE(selection_for({24'000}, 192'000, 44'100).has_value());
  CHECK_FALSE(selection_for({24'000}, 192'000, sample_rate, 1).has_value());
  CHECK_FALSE(selection_for({0}, 192'000).has_value());
  CHECK_FALSE(selection_for({}, 192'000).has_value());

  const std::vector<OutputCandidate> mixed = {
      {.bytes_per_second = 12'000, .sample_rate = 44'100, .channels = 2},
      {.bytes_per_second = 24'000, .sample_rate = 48'000, .channels = 2},
  };
  CHECK(select_candidate(mixed, 192'000, sample_rate, channels) == 1U);

  // 第三方编码器可能不声明采样率/声道，不能因此丢掉它自己给出的合法类型
  const std::vector<OutputCandidate> undeclared = {
      {.bytes_per_second = 24'000, .sample_rate = 0, .channels = 0},
  };
  CHECK(select_candidate(undeclared, 192'000, sample_rate, channels) == 0U);
}
