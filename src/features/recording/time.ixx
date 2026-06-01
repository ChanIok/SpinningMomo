module;

export module Features.Recording.Time;

import std;

namespace Features::Recording::Time {

// 统一把 QPC 转成 100ns，录制时间线、音频时间戳、收尾目标都基于同一套时钟。
export auto query_qpc_100ns() -> std::int64_t;

// 计算“距离录制起点过去了多久”；起点无效时返回 0。
export auto elapsed_since_start_100ns(std::int64_t start_qpc_100ns) -> std::int64_t;

// 把一个绝对 QPC 时间戳换算成“相对录制起点”的时间线。
export auto relative_timestamp_100ns(std::int64_t start_qpc_100ns, std::int64_t absolute_qpc_100ns)
    -> std::int64_t;

}  // namespace Features::Recording::Time
