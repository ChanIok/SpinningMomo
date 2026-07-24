#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "ui/floating_window/events.hpp"

namespace features::window_control {

// 处理比例改变事件
auto handle_ratio_changed(core::AppState& state,
                          const ui::floating_window::events::RatioChangeEvent& event) -> void;

// 处理分辨率改变事件
auto handle_resolution_changed(core::AppState& state,
                               const ui::floating_window::events::ResolutionChangeEvent& event)
    -> void;

// 处理窗口选择事件
auto handle_window_selected(core::AppState& state,
                            const ui::floating_window::events::WindowSelectionEvent& event) -> void;

// 重置窗口变换（直接调用版本）
auto reset_window_transform(core::AppState& state) -> void;

}  // namespace features::window_control
