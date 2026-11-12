#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::preview {

// 切换预览功能
auto toggle_preview(core::AppState& state) -> void;

}  // namespace features::preview
