#pragma once

#include "vendor/std.hpp"

namespace features::adb_mode {

// 表示 ADB 模式从未连接、正在连接到已连接或恢复中的生命周期状态。
enum class ConnectionState : std::uint8_t {
  Disconnected,
  Connecting,
  Connected,
  Restoring,
  Error,
};

// 描述一个显示尺寸，宽高单位都是像素。
struct Resolution {
  int width = 0;
  int height = 0;

  auto operator==(const Resolution& other) const noexcept -> bool {
    return width == other.width && height == other.height;
  }
};

// 描述一次 ADB 命令连接所需的可执行文件、地址和设备序列号。
struct AdbConnectionConfig {
  std::filesystem::path executable;
  std::string host = "127.0.0.1";
  int port = 7555;
  // 为空时由连接流程根据 host/port 或发现结果选择设备。
  std::string serial;
};

// adb devices 输出中的单个设备记录。
struct AdbDevice {
  std::string serial;
  std::string state;
};

// 记录连接成功的设备，以及本次是否由模块建立了 TCP 连接。
struct AdbConnectionResult {
  std::string serial;
  bool connected_by_us = false;
};

// 用于 RPC 和前端通知的稳定状态快照，不暴露内部互斥量或恢复文件格式。
struct AdbModeStatus {
  bool connected = false;
  bool operation_in_progress = false;
  std::string connection_state;
  std::string adb_path;
  std::string serial;
  std::string host = "127.0.0.1";
  int port = 7555;
  bool restore_pending = false;
  int display_width = 0;
  int display_height = 0;
  std::string last_error;
};

// 将内部连接状态转换成前端和通知使用的稳定字符串。
auto connection_state_to_string(ConnectionState state) -> std::string;

}  // namespace features::adb_mode
