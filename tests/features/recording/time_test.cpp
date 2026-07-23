#include <doctest/doctest.h>

import Features.Recording.Time;

using Features::Recording::Time::relative_timestamp_100ns;

// 无效录制起点不能生成可供编码器使用的时间线
TEST_CASE("relative timestamp returns zero for an invalid recording start") {
  CHECK(relative_timestamp_100ns(0, 100) == 0);
  CHECK(relative_timestamp_100ns(-1, 100) == 0);
}

// 起点之前的采样统一钳制为零，避免产生负时间戳
TEST_CASE("relative timestamp never becomes negative") {
  CHECK(relative_timestamp_100ns(100, 99) == 0);
  CHECK(relative_timestamp_100ns(100, 100) == 0);
}

// 有效采样使用统一的录制起点换算相对偏移
TEST_CASE("relative timestamp is the difference from recording start") {
  CHECK(relative_timestamp_100ns(100, 135) == 35);
}
