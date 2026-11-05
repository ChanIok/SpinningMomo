#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/audioclient.hpp"
#include "vendor/windows/mmdeviceapi.hpp"

namespace utils::media::audio_capture {

// 音频源类型
enum class AudioSource {
  None,     // 不录制音频
  System,   // 系统全部音频（传统 Loopback）
  GameOnly  // 仅游戏音频（Process Loopback，需 Windows 10 2004+）
};

// 从字符串转换为 AudioSource
constexpr AudioSource audio_source_from_string(std::string_view str) {
  if (str == "none") return AudioSource::None;
  if (str == "game_only") return AudioSource::GameOnly;
  return AudioSource::System;  // 默认
}

// 两种loopback都显式协商48kHz/stereo/s16，设备变化不改变编码流格式。
struct AudioCaptureContext {
  wil::com_ptr<IMMDevice> device;
  wil::com_ptr<IAudioClient> audio_client;
  wil::com_ptr<IAudioCaptureClient> capture_client;
  wil::unique_cotaskmem_ptr<WAVEFORMATEX> wave_format;
  UINT32 buffer_frame_count = 0;
  wil::unique_handle audio_event, stop_event, restart_event;
  std::jthread capture_thread;
};
// 数据只在回调期间有效；QPC是包首采样时刻，单位100ns。
using AudioPacketCallback = std::function<void(const BYTE*, UINT32, UINT64, UINT64, DWORD)>;
using CaptureStateCallback = std::function<void(bool, const std::string&)>;
auto is_process_loopback_supported() -> bool;
auto start(AudioCaptureContext& ctx, AudioSource source, std::uint32_t process_id,
           AudioPacketCallback on_packet, CaptureStateCallback on_state)
    -> std::expected<void, std::string>;
auto stop(AudioCaptureContext& ctx) -> void;
}  // namespace utils::media::audio_capture
