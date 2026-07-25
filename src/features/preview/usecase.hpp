#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "ui/floating_window/events.hpp"

namespace features::preview {

// 切换预览功能
auto toggle_preview(core::AppState& state) -> void;

}  // namespace features::preview
