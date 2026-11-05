#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "utils/media/mft.hpp"

namespace features::recording::mux {

// 将压缩数据包提交到待封装队列，校验内存上限并记录告警水位。
auto enqueue(core::AppState& app_state, unsigned stream, utils::media::mft::Packet packet)
    -> std::expected<void, std::string>;

// 封装工作线程主循环：创建分片 MP4 写入器 → 轮询并交错写入音视频压缩包 → 最终收尾与索引整理
auto run(core::AppState& app_state) -> std::expected<void, std::string>;

}  // namespace features::recording::mux
