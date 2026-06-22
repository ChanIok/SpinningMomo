module;

module Features.Photography.LongExposure;

import std;

namespace Features::Photography::LongExposure {

// 滑块档位：0=关闭，30/60/120/300/1000 帧
constexpr std::array<int, 6> kLongExposureFrameStops = {0, 30, 60, 120, 300, 1000};

auto frame_stops() -> std::span<const int> { return kLongExposureFrameStops; }

// 找到离输入值最近的档位，用于滑块吸附
auto nearest_frame_stop(int frames) -> int {
  int nearest = kLongExposureFrameStops.front();
  int nearest_distance = std::abs(frames - nearest);
  for (int stop : kLongExposureFrameStops) {
    const int distance = std::abs(frames - stop);
    if (distance < nearest_distance) {
      nearest = stop;
      nearest_distance = distance;
    }
  }
  return nearest;
}

}  // namespace Features::Photography::LongExposure
