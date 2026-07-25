#pragma once

#include "vendor/std.hpp"

namespace features::photography::long_exposure {

auto frame_stops() -> std::span<const int>;
auto nearest_frame_stop(int frames) -> int;

}  // namespace features::photography::long_exposure
