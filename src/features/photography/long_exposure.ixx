module;

export module Features.Photography.LongExposure;

import std;

namespace Features::Photography::LongExposure {

export auto frame_stops() -> std::span<const int>;
export auto nearest_frame_stop(int frames) -> int;

}  // namespace Features::Photography::LongExposure
