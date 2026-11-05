#include "vendor/std.hpp"

#include "vendor/doctest.hpp"

#include "features/recording/time.hpp"

using features::recording::time::relative_timestamp_100ns;

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

TEST_CASE("video timestamps use the total frame index without accumulating rounding") {
  using features::recording::time::timestamp_for_index;
  CHECK(timestamp_for_index(1, 300) == 33'333);
  CHECK(timestamp_for_index(3, 300) == 100'000);
  CHECK(timestamp_for_index(300 * 3600, 300) == 36'000'000'000LL);
  CHECK(timestamp_for_index(60, 60) == 10'000'000);
}

TEST_CASE("PCM sample clock and pre-start trimming preserve signed positions") {
  using features::recording::time::sample_index_at;
  using features::recording::time::timestamp_for_index;
  CHECK(timestamp_for_index(480, 48000) == 100'000);
  CHECK(timestamp_for_index(4'800'000, 48000) == 1'000'000'000);
  CHECK(sample_index_at(100'000, 48000) == 480);
  CHECK(sample_index_at(-100'000, 48000) == -480);
  CHECK(sample_index_at(-1, 48000) == -1);
}
