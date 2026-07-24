#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace ui::context_menu::interaction {

enum class TimerActionType { None, ShowSubmenu, HideSubmenu };

struct TimerAction {
  TimerActionType type = TimerActionType::None;
  int parent_index = -1;
  bool invalidate_main = false;
};

auto reset(core::AppState& state) -> void;

auto cancel_pending_intent(core::AppState& state, HWND timer_owner) -> void;

auto on_main_mouse_move(core::AppState& state, int hover_index, HWND timer_owner) -> bool;

auto on_submenu_mouse_move(core::AppState& state, int submenu_hover_index, HWND timer_owner)
    -> bool;

auto on_mouse_leave(core::AppState& state, HWND source_hwnd, HWND timer_owner) -> bool;

auto on_timer(core::AppState& state, HWND timer_owner, WPARAM timer_id) -> TimerAction;

auto get_main_highlight_index(const core::AppState& state) -> int;

}  // namespace ui::context_menu::interaction
