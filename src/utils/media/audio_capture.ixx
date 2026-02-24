module;

export module Utils.Media.AudioCapture;

import std;
import <audioclient.h>;
import <mmdeviceapi.h>;
import <wil/com.h>;
import <windows.h>;

namespace Utils::Media::AudioCapture {

// 音频源类型
export enum class AudioSource {
  None,     // 不录制音频
  System,   // 系统全部音频（传统 Loopback）
  GameOnly  // 仅游戏音频（Process Loopback，需 Windows 10 2004+）
};

// 从字符串转换为 AudioSource
export constexpr AudioSource audio_source_from_string(std::string_view str) {
  if (str == "none") return AudioSource::None;
  if (str == "game_only") return AudioSource::GameOnly;
  return AudioSource::System;  // 默认
}

// 音频捕获上下文
export struct AudioCaptureContext {
  wil::com_ptr<IMMDevice> device;                    // 音频设备
  wil::com_ptr<IAudioClient> audio_client;           // 音频客户端
  wil::com_ptr<IAudioCaptureClient> capture_client;  // 捕获客户端

  WAVEFORMATEX* wave_format = nullptr;  // 音频格式
  UINT32 buffer_frame_count = 0;        // 缓冲区帧数

  HANDLE audio_event = nullptr;           // WASAPI 缓冲就绪事件
  std::jthread capture_thread;            // 捕获线程
  std::atomic<bool> should_stop = false;  // 停止信号
};

// 音频数据包回调: (audio_data, num_frames, bytes_per_frame, timestamp_100ns)
export using AudioPacketCallback = std::function<void(const BYTE*, UINT32, UINT32, std::int64_t)>;

// 是否支持 Process Loopback API（Windows 10 2004+）
export auto is_process_loopback_supported() -> bool;

// 初始化音频捕获（根据音频源类型选择不同的初始化方式）
export auto initialize(AudioCaptureContext& ctx, AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string>;

// 启动音频捕获线程（回调式）
// get_elapsed_100ns: 获取当前经过时间（100ns 单位）
// is_active: 判断调用方是否仍处于活跃状态
// on_packet: 音频数据包回调
export auto start_capture_thread(AudioCaptureContext& ctx,
                                 std::function<std::int64_t()> get_elapsed_100ns,
                                 std::function<bool()> is_active, AudioPacketCallback on_packet)
    -> void;

// 停止音频捕获
export auto stop(AudioCaptureContext& ctx) -> void;

// 清理音频资源
export auto cleanup(AudioCaptureContext& ctx) -> void;

}  // namespace Utils::Media::AudioCapture
