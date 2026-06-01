module;

// Features.Recording.Time
//
// 录制模块统一的时间线工具，基于 Windows QPC（QueryPerformanceCounter）实现。
// 录制过程中所有时间相关计算——视频帧时间戳、音频时间戳、收尾目标——
// 都以 100ns 为单位，统一通过此模块换算。
//
// 设计要点：
// - 使用 100ns 单位（10 MHz 时钟），与 Media Foundation 的 sample 时间单位一致。
// - "录制起点"（start_qpc_100ns）在 start() 中冻结，后续所有时间都是相对偏移。
// - 音频采集线程和 WGC 帧回调都使用同一个时钟基准，保证音画同步。

module Features.Recording.Time;

import std;
import <windows.h>;

namespace Features::Recording::Time {

// 查询当前 QPC 计数，换算成 100ns 单位。
// QPC 是 Windows 提供的高精度单调时钟，精度通常在微秒级。
// 返回值：100ns 为单位的绝对时间戳，查询失败时返回 0。
auto query_qpc_100ns() -> std::int64_t {
  LARGE_INTEGER counter{};
  LARGE_INTEGER frequency{};
  if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
    return 0;
  }

  constexpr long double k_hundred_ns_per_second = 10'000'000.0L;
  return static_cast<std::int64_t>(counter.QuadPart * k_hundred_ns_per_second / frequency.QuadPart);
}

// 计算"从录制起点到此刻"经过了多少 100ns。
// 等价于 relative_timestamp_100ns(start, 当前QPC)。
// 编码线程用它来判断"现在时间线走到哪了"，以决定是否该补帧。
auto elapsed_since_start_100ns(std::int64_t start_qpc_100ns) -> std::int64_t {
  return relative_timestamp_100ns(start_qpc_100ns, query_qpc_100ns());
}

// 把一个绝对 QPC 时间戳转换成"相对录制起点"的偏移。
// 如果起点无效或绝对时间在起点之前，返回 0（避免负时间戳导致编码器异常）。
// 音频线程把 WASAPI 回馈的 QPC 位置传进来换算，统一到录制时间线上。
auto relative_timestamp_100ns(std::int64_t start_qpc_100ns, std::int64_t absolute_qpc_100ns)
    -> std::int64_t {
  if (start_qpc_100ns <= 0 || absolute_qpc_100ns <= start_qpc_100ns) {
    return 0;
  }
  return absolute_qpc_100ns - start_qpc_100ns;
}

}  // namespace Features::Recording::Time
