#pragma once

#include "vendor/std.hpp"

namespace features::recording::time {

// 从总帧/采样序号计算时间，不能重复累加已经取整的单帧时长。
auto timestamp_for_index(std::uint64_t index, std::uint32_t rate) -> std::int64_t;

// 保留起点前的负位置，供音频裁掉前缀；不将整包负时间戳钳制成零。
auto sample_index_at(std::int64_t timestamp_100ns, std::uint32_t rate) -> std::int64_t;

// 统一把 QPC 转成 100ns，录制时间线、音频时间戳、收尾目标都基于同一套时钟。
auto query_qpc_100ns() -> std::int64_t;

// 计算“距离录制起点过去了多久”；起点无效时返回 0。
auto elapsed_since_start_100ns(std::int64_t start_qpc_100ns) -> std::int64_t;

// 把一个绝对 QPC 时间戳换算成“相对录制起点”的时间线。
auto relative_timestamp_100ns(std::int64_t start_qpc_100ns, std::int64_t absolute_qpc_100ns)
    -> std::int64_t;

}  // namespace features::recording::time
