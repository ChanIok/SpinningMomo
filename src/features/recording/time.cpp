#include "features/recording/time.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace features::recording::time {

// 根据帧或采样总序号计算绝对时间戳（100ns）：避免累加单帧取整误差
auto timestamp_for_index(std::uint64_t index, std::uint32_t rate) -> std::int64_t {
  if (!rate) {
    return 0;
  }

  // 先除后乘整秒部分，再加上余数折算的 100ns，防止 64 位整型溢出
  return static_cast<std::int64_t>(index / rate * 10'000'000 + index % rate * 10'000'000 / rate);
}

// 根据时间戳（100ns）换算采样序号：保留负数序号以供裁剪录制起点前的音频前缀
auto sample_index_at(std::int64_t timestamp_100ns, std::uint32_t rate) -> std::int64_t {
  const auto remainder = timestamp_100ns % 10'000'000 * rate;

  // 向下取整计算，包含负数时间戳的处理
  return timestamp_100ns / 10'000'000 * rate + remainder / 10'000'000 -
         (remainder < 0 && remainder % 10'000'000 != 0 ? 1 : 0);
}

// 查询当前 QPC 单调时钟并换算为 100ns 单位
auto query_qpc_100ns() -> std::int64_t {
  LARGE_INTEGER counter{};
  LARGE_INTEGER frequency{};

  // 查询硬件高精度时钟计数与频率
  if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
    return 0;
  }

  constexpr long double k_hundred_ns_per_second = 10'000'000.0L;
  return static_cast<std::int64_t>(counter.QuadPart * k_hundred_ns_per_second / frequency.QuadPart);
}

// 计算从录制起点到当前时刻经过的 100ns 时长
auto elapsed_since_start_100ns(std::int64_t start_qpc_100ns) -> std::int64_t {
  return relative_timestamp_100ns(start_qpc_100ns, query_qpc_100ns());
}

// 将绝对 QPC 时间戳转换为相对录制起点的非负偏移量
auto relative_timestamp_100ns(std::int64_t start_qpc_100ns, std::int64_t absolute_qpc_100ns)
    -> std::int64_t {
  // 起点无效或时间点在起点之前时归零
  if (start_qpc_100ns <= 0 || absolute_qpc_100ns <= start_qpc_100ns) {
    return 0;
  }

  return absolute_qpc_100ns - start_qpc_100ns;
}

}  // namespace features::recording::time
