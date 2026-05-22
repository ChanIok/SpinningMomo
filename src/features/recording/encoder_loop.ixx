module;

export module Features.Recording.EncoderLoop;

import std;
import Core.State;

namespace Features::Recording::EncoderLoop {

// WGC 来帧时由回调里调用：打个标记并唤醒编码线程，真正的取帧在编码线程里做。
export auto mark_video_frame_pending(Core::State::AppState& app_state) -> void;

// start() 里会卡住等这里：编码线程把 SinkWriter 建好之后才允许开始捕获。
export auto wait_encoder_ready(Core::State::AppState& app_state) -> void;

// 停止录制时调用：告诉编码线程别再接新数据，把池子和队列里剩的写完再 finalize。
export auto signal_encoder_finish(Core::State::AppState& app_state) -> void;

// 编码线程本体：唯一创建 MF 编码器、写视频/音频 sample 的线程。
export auto encoder_thread_proc(Core::State::AppState& app_state, std::stop_token stop_token,
                                std::function<void()> request_resize_restart) -> void;

}  // namespace Features::Recording::EncoderLoop
