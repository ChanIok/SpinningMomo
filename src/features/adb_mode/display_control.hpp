#pragma once

#include "vendor/std.hpp"

#include "features/adb_mode/types.hpp"
#include "features/window_control/types.hpp"

namespace features::adb_mode::display_control {

// 查询设备的物理显示尺寸。
auto query(const AdbConnectionConfig& config, std::string_view serial)
    -> std::expected<Resolution, std::string>;

// 使用 adb shell wm size 设置设备的临时显示尺寸。
auto set(const AdbConnectionConfig& config, std::string_view serial, const Resolution& size)
    -> std::expected<void, std::string>;

// 使用 adb shell wm size reset 清除设备的临时显示覆盖并恢复系统默认尺寸。
auto reset(const AdbConnectionConfig& config, std::string_view serial)
    -> std::expected<void, std::string>;

// ADB 模式沿用现有预设的面积/短边计算，但以设备物理尺寸为基准。
auto calculate_target_resolution(double ratio,
                                 const features::window_control::ResolutionPresetInput& preset,
                                 const Resolution& base, bool align_to_8, bool use_short_edge)
    -> Resolution;

}  // namespace features::adb_mode::display_control
