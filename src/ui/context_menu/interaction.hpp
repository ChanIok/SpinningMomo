#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"

namespace UI::ContextMenu::Interaction {

enum class TimerActionType { None, ShowSubmenu, HideSubmenu };

struct TimerAction {
  TimerActionType type = TimerActionType::None;
  int parent_index = -1;
  bool invalidate_main = false;
};

auto reset(Core::State::AppState& state) -> void;

auto cancel_pending_intent(Core::State::AppState& state, HWND timer_owner) -> void;

auto on_main_mouse_move(Core::State::AppState& state, int hover_index, HWND timer_owner) -> bool;

auto on_submenu_mouse_move(Core::State::AppState& state, int submenu_hover_index, HWND timer_owner)
    -> bool;

auto on_mouse_leave(Core::State::AppState& state, HWND source_hwnd, HWND timer_owner) -> bool;

auto on_timer(Core::State::AppState& state, HWND timer_owner, WPARAM timer_id) -> TimerAction;

auto get_main_highlight_index(const Core::State::AppState& state) -> int;

}  // namespace UI::ContextMenu::Interaction
