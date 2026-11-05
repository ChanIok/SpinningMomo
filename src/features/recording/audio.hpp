#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::recording::audio {

// 音频工作线程主循环：配置 WASAPI 采集 → 连续 PCM 时钟与静音填充 → AAC MFT 编码 → 交付封装队列
auto run(core::AppState& app_state) -> std::expected<void, std::string>;

}  // namespace features::recording::audio
