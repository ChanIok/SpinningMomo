module;

module Core.RuntimeInfo;

import std;
import Core.State;
import Core.State.RuntimeInfo;
import Core.WebView;
import Utils.Graphics.Capture;
import Utils.Logger;
import Utils.Media.AudioCapture;
import Utils.System;
import Vendor.BuildConfig;
import Vendor.Version;

namespace Core::RuntimeInfo::Detail {

using RuntimeInfoState = Core::State::RuntimeInfo::RuntimeInfoState;

auto collect_app_version(RuntimeInfoState& runtime_info) -> void {
  runtime_info.version = Vendor::Version::get_app_version();

  auto parse_result = std::istringstream(runtime_info.version);
  char dot = '.';
  unsigned int major = 0;
  unsigned int minor = 0;
  unsigned int patch = 0;
  unsigned int build = 0;
  if (parse_result >> major >> dot >> minor >> dot >> patch >> dot >> build) {
    runtime_info.major_version = major;
    runtime_info.minor_version = minor;
    runtime_info.patch_version = patch;
    runtime_info.build_number = build;
  }
}

auto collect_os_and_capabilities(RuntimeInfoState& runtime_info) -> void {
  auto version_result = Utils::System::get_windows_version();
  if (!version_result) {
    Logger().error("Failed to get OS version: {}", version_result.error());
    return;
  }

  const auto& version = version_result.value();
  Logger().info("OS Version: {}.{}.{}", version.major_version, version.minor_version,
                version.build_number);

  runtime_info.os_major_version = version.major_version;
  runtime_info.os_minor_version = version.minor_version;
  runtime_info.os_build_number = version.build_number;
  runtime_info.os_name = Utils::System::get_windows_name(version);

  // Windows Graphics Capture 支持 (Windows 10 1903 / 18362+)
  runtime_info.is_capture_supported =
      (version.major_version > 10) ||
      (version.major_version == 10 && version.build_number >= 18362);
  runtime_info.is_cursor_capture_control_supported =
      Utils::Graphics::Capture::is_cursor_capture_control_supported();
  runtime_info.is_border_control_supported =
      Utils::Graphics::Capture::is_border_control_supported();

  // Process Loopback 音频支持 (Windows 10 2004 / 19041+)
  runtime_info.is_process_loopback_audio_supported =
      Utils::Media::AudioCapture::is_process_loopback_supported();

  Logger().info("Capture support: {}, cursor control: {}, border control: {}, process loopback: {}",
                runtime_info.is_capture_supported, runtime_info.is_cursor_capture_control_supported,
                runtime_info.is_border_control_supported,
                runtime_info.is_process_loopback_audio_supported);
}

auto collect_webview_info(RuntimeInfoState& runtime_info) -> void {
  auto webview2_version = Core::WebView::get_runtime_version();
  if (!webview2_version) {
    runtime_info.is_webview2_available = false;
    runtime_info.webview2_version.clear();
    Logger().warn("WebView2 runtime not available: {}", webview2_version.error());
    return;
  }

  runtime_info.is_webview2_available = true;
  runtime_info.webview2_version = webview2_version.value();
  Logger().info("WebView2 runtime: {}", runtime_info.webview2_version);
}

}  // namespace Core::RuntimeInfo::Detail

namespace Core::RuntimeInfo {

auto collect(Core::State::AppState& app_state) -> void {
  if (!app_state.runtime_info) {
    return;
  }

  auto& runtime_info = *app_state.runtime_info;
  runtime_info.is_debug_build = Vendor::BuildConfig::is_debug_build();
  Detail::collect_app_version(runtime_info);
  Detail::collect_os_and_capabilities(runtime_info);
  Detail::collect_webview_info(runtime_info);
}

}  // namespace Core::RuntimeInfo
