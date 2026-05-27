module;

export module Utils.Media.AudioCapture;

import std;
import <audioclient.h>;
import <mmdeviceapi.h>;
import <wil/com.h>;
import <wil/resource.h>;

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

  wil::unique_cotaskmem_ptr<WAVEFORMATEX> wave_format;  // 音频格式
  UINT32 buffer_frame_count = 0;                        // 缓冲区帧数

  wil::unique_handle audio_event;  // WASAPI 缓冲就绪事件
  wil::unique_handle stop_event;   // 捕获线程停止唤醒事件
  std::jthread capture_thread;     // 捕获线程
};

// 音频数据包回调:
// (audio_data, num_frames, bytes_per_frame, qpc_position_100ns, flags)
// qpc_position_100ns 是 WASAPI 给出的包首帧 QPC 时间，单位为 100ns。
export using AudioPacketCallback = std::function<void(const BYTE*, UINT32, UINT32, UINT64, DWORD)>;

// 是否支持 Process Loopback API（Windows 10 2004+）
export auto is_process_loopback_supported() -> bool;

// 初始化音频捕获（根据音频源类型选择不同的初始化方式）
export auto initialize(AudioCaptureContext& ctx, AudioSource source, std::uint32_t process_id)
    -> std::expected<void, std::string>;

// 启动音频捕获线程（回调式）
// on_packet: 音频数据包回调；模块本身只负责产出 PCM 包，不感知上层录制状态。
export auto start_capture_thread(AudioCaptureContext& ctx, AudioPacketCallback on_packet) -> void;

// 停止音频捕获
export auto stop(AudioCaptureContext& ctx) -> void;

// 清理音频资源
export auto cleanup(AudioCaptureContext& ctx) -> void;

}  // namespace Utils::Media::AudioCapture
