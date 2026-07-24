#pragma once

namespace Features::Photography::LongExposure {

auto frame_stops() -> std::span<const int>;
auto nearest_frame_stop(int frames) -> int;

}  // namespace Features::Photography::LongExposure
