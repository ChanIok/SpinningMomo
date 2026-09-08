#pragma once

#include "vendor/std.hpp"

#include "features/adb_mode/types.hpp"

namespace features::adb_mode::device_finder {

// 补全连接配置中的 ADB 路径和设备序列号；自动模式只识别正在运行的 MuMu、雷电和蓝叠。
// executable 非空时仅使用用户指定的 ADB。
auto resolve_connection(AdbConnectionConfig config)
    -> std::expected<AdbConnectionConfig, std::string>;

// 扫描并发现所有当前可用的 Android 设备（包括运行中模拟器与已识别的真机）。
auto discover_all_devices(AdbConnectionConfig config)
    -> std::expected<std::vector<DiscoveredAdbDevice>, std::string>;

}  // namespace features::adb_mode::device_finder
