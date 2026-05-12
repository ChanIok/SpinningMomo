module;

export module Features.Recording.EncoderLoop;

import std;
import Features.Recording.State;

namespace Features::Recording::EncoderLoop {

// WGC 帧到达时标记有视频帧待处理，并唤醒编码线程。
export auto mark_video_frame_pending(Features::Recording::State::RecordingState& state) -> void;

// 阻塞等待编码线程完成 SinkWriter 创建，供 start() 判断能否继续捕获。
export auto wait_encoder_ready(Features::Recording::State::RecordingState& state) -> void;

// 通知编码线程停止接收新输入，并排空队列后 finalize。
export auto signal_encoder_finish(Features::Recording::State::RecordingState& state) -> void;

// 编码线程入口；唯一创建和写入 Media Foundation SinkWriter 的执行路径。
export auto encoder_thread_proc(Features::Recording::State::RecordingState& state,
                                std::stop_token stop_token,
                                std::function<void()> request_resize_restart) -> void;

}  // namespace Features::Recording::EncoderLoop
