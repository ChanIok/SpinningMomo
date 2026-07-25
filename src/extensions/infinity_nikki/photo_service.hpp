#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace extensions::infinity_nikki::photo_service {

// 根据当前设置决定是否向 Gallery.Watcher 注册无限暖暖目录监听。
// 监听触发扫描后，回调驱动媒体硬链接同步与照片元数据提取。
// 须在 features::gallery::initialize 之后调用。
auto register_from_settings(core::AppState& app_state) -> void;

auto refresh_from_settings(core::AppState& app_state) -> void;

auto shutdown(core::AppState& app_state) -> void;

}  // namespace extensions::infinity_nikki::photo_service
