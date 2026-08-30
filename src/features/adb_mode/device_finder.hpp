#pragma once

#include "vendor/std.hpp"

#include "features/adb_mode/types.hpp"

namespace features::adb_mode::device_finder {

// 补全连接配置中的 ADB 路径和设备序列号；自动模式只识别正在运行的模拟器。
// executable 非空时仅使用用户指定的 ADB。
auto resolve_connection(AdbConnectionConfig config)
    -> std::expected<AdbConnectionConfig, std::string>;

}  // namespace features::adb_mode::device_finder
