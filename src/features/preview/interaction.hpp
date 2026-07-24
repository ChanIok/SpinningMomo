#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace features::preview::interaction {

// 主消息处理函数
// 返回值：first为true表示已处理消息，为false表示应使用默认处理
auto handle_preview_message(core::AppState& state, HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) -> std::pair<bool, LRESULT>;

}  // namespace features::preview::interaction
