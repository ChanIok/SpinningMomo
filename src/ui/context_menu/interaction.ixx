module;

#include <windows.h>

export module UI.ContextMenu.Interaction;

import Core.State;

export namespace UI::ContextMenu::Interaction {

enum class TimerActionType { None, ShowSubmenu, HideSubmenu };

struct TimerAction {
  TimerActionType type = TimerActionType::None;
  int parent_index = -1;
  bool invalidate_main = false;
};

export auto reset(Core::State::AppState& state) -> void;

export auto cancel_pending_intent(Core::State::AppState& state, HWND timer_owner) -> void;

export auto on_main_mouse_move(Core::State::AppState& state, int hover_index, HWND timer_owner)
    -> bool;

export auto on_submenu_mouse_move(Core::State::AppState& state, int submenu_hover_index,
                                  HWND timer_owner) -> bool;

export auto on_mouse_leave(Core::State::AppState& state, HWND source_hwnd, HWND timer_owner)
    -> bool;

export auto on_timer(Core::State::AppState& state, HWND timer_owner, WPARAM timer_id)
    -> TimerAction;

export auto get_main_highlight_index(const Core::State::AppState& state) -> int;

}  // namespace UI::ContextMenu::Interaction
