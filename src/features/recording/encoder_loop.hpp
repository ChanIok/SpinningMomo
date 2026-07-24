#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::recording::encoder_loop {

// WGC 来帧时由回调里调用：打个标记并唤醒编码线程，真正的取帧在编码线程里做。
auto mark_video_frame_pending(core::AppState& app_state) -> void;

// start() 里会卡住等这里：编码线程把 SinkWriter 建好之后才允许开始捕获。
auto wait_encoder_ready(core::AppState& app_state) -> void;

// 停止录制时调用：告诉编码线程别再接新数据，把池子和队列里剩的写完再 finalize。
auto signal_encoder_finish(core::AppState& app_state) -> void;

// 编码线程本体：唯一创建 MF 编码器、写视频/音频 sample 的线程。
auto encoder_thread_proc(core::AppState& app_state, std::stop_token stop_token,
                         std::function<void()> request_resize_restart) -> void;

}  // namespace features::recording::encoder_loop
