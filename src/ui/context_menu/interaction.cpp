module;

#include <windows.h>

module UI.ContextMenu.Interaction;

import std;
import Core.State;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;

namespace {

using UI::ContextMenu::State::ContextMenuState;
using UI::ContextMenu::Types::PendingIntentType;

auto resolve_timer_owner(const ContextMenuState& menu_state, HWND fallback) -> HWND {
  return menu_state.hwnd ? menu_state.hwnd : fallback;
}

auto cancel_pending_intent_impl(ContextMenuState& menu_state, HWND timer_owner) -> void {
  auto& interaction = menu_state.interaction;
  if (interaction.intent_timer_id != 0) {
    KillTimer(timer_owner, interaction.INTENT_TIMER_ID);
    interaction.intent_timer_id = 0;
  }
  interaction.pending_intent = PendingIntentType::None;
  interaction.pending_parent_index = -1;
}

auto request_intent(ContextMenuState& menu_state, HWND timer_owner, PendingIntentType intent,
                    int parent_index, UINT delay_ms) -> void {
  auto& interaction = menu_state.interaction;

  if (intent == PendingIntentType::None) {
    cancel_pending_intent_impl(menu_state, timer_owner);
    return;
  }

  // 相同意图保持原定时器，避免高频WM_MOUSEMOVE导致延迟被不断重置。
  if (interaction.intent_timer_id != 0 && interaction.pending_intent == intent &&
      interaction.pending_parent_index == parent_index) {
    return;
  }

  cancel_pending_intent_impl(menu_state, timer_owner);
  interaction.pending_intent = intent;
  interaction.pending_parent_index = parent_index;
  interaction.intent_timer_id =
      SetTimer(timer_owner, interaction.INTENT_TIMER_ID, delay_ms, nullptr);

  if (interaction.intent_timer_id == 0) {
    interaction.pending_intent = PendingIntentType::None;
    interaction.pending_parent_index = -1;
  }
}

}  // anonymous namespace

namespace UI::ContextMenu::Interaction {

auto reset(Core::State::AppState& state) -> void { state.context_menu->interaction = {}; }

auto cancel_pending_intent(Core::State::AppState& state, HWND timer_owner) -> void {
  auto& menu_state = *state.context_menu;
  cancel_pending_intent_impl(menu_state, resolve_timer_owner(menu_state, timer_owner));
}

auto on_main_mouse_move(Core::State::AppState& state, int hover_index, HWND timer_owner) -> bool {
  auto& menu_state = *state.context_menu;
  auto& interaction = menu_state.interaction;
  timer_owner = resolve_timer_owner(menu_state, timer_owner);
  const auto previous_zone = interaction.cursor_zone;
  interaction.cursor_zone = Types::CursorZone::MainMenu;

  bool should_repaint = previous_zone != Types::CursorZone::MainMenu && menu_state.submenu_hwnd &&
                        menu_state.submenu_parent_index >= 0 &&
                        menu_state.submenu_parent_index < static_cast<int>(menu_state.items.size());

  if (hover_index != interaction.hover_index) {
    interaction.hover_index = hover_index;
    should_repaint = true;
  }

  if (hover_index < 0 || hover_index >= static_cast<int>(menu_state.items.size())) {
    if (menu_state.submenu_hwnd) {
      request_intent(menu_state, timer_owner, PendingIntentType::HideSubmenu, -1,
                     interaction.HIDE_SUBMENU_DELAY);
    } else {
      request_intent(menu_state, timer_owner, PendingIntentType::None, -1, 0);
    }
    return should_repaint;
  }

  const auto& item = menu_state.items[hover_index];
  if (!item.has_submenu()) {
    if (menu_state.submenu_hwnd) {
      request_intent(menu_state, timer_owner, PendingIntentType::HideSubmenu, -1,
                     interaction.HIDE_SUBMENU_DELAY);
    } else {
      request_intent(menu_state, timer_owner, PendingIntentType::None, -1, 0);
    }
    return should_repaint;
  }

  if (menu_state.submenu_hwnd && menu_state.submenu_parent_index == hover_index) {
    request_intent(menu_state, timer_owner, PendingIntentType::None, -1, 0);
    return should_repaint;
  }

  const bool has_open_submenu = menu_state.submenu_hwnd != nullptr;
  request_intent(
      menu_state, timer_owner,
      has_open_submenu ? PendingIntentType::SwitchSubmenu : PendingIntentType::OpenSubmenu,
      hover_index,
      has_open_submenu ? interaction.SWITCH_SUBMENU_DELAY : interaction.OPEN_SUBMENU_DELAY);

  return should_repaint;
}

auto on_submenu_mouse_move(Core::State::AppState& state, int submenu_hover_index, HWND timer_owner)
    -> bool {
  auto& menu_state = *state.context_menu;
  auto& interaction = menu_state.interaction;
  timer_owner = resolve_timer_owner(menu_state, timer_owner);
  interaction.cursor_zone = Types::CursorZone::Submenu;

  bool should_repaint = false;
  if (submenu_hover_index != interaction.submenu_hover_index) {
    interaction.submenu_hover_index = submenu_hover_index;
    should_repaint = true;
  }

  // 进入当前子菜单即取消任何待处理意图（切换/隐藏）。
  request_intent(menu_state, timer_owner, PendingIntentType::None, -1, 0);
  return should_repaint;
}

auto on_mouse_leave(Core::State::AppState& state, HWND source_hwnd, HWND timer_owner) -> bool {
  auto& menu_state = *state.context_menu;
  auto& interaction = menu_state.interaction;
  timer_owner = resolve_timer_owner(menu_state, timer_owner);

  const auto previous_zone = interaction.cursor_zone;
  interaction.cursor_zone = Types::CursorZone::Outside;
  bool should_repaint = false;

  if (source_hwnd == menu_state.hwnd && previous_zone == Types::CursorZone::MainMenu &&
      menu_state.submenu_hwnd && menu_state.submenu_parent_index >= 0 &&
      menu_state.submenu_parent_index < static_cast<int>(menu_state.items.size())) {
    should_repaint = true;
  }

  if (source_hwnd == menu_state.submenu_hwnd) {
    if (interaction.submenu_hover_index != -1) {
      interaction.submenu_hover_index = -1;
      should_repaint = true;
    }
  } else if (source_hwnd == menu_state.hwnd) {
    if (interaction.hover_index != -1) {
      interaction.hover_index = -1;
      should_repaint = true;
    }
  }

  if (menu_state.submenu_hwnd) {
    request_intent(menu_state, timer_owner, PendingIntentType::HideSubmenu, -1,
                   interaction.HIDE_SUBMENU_DELAY);
  } else {
    request_intent(menu_state, timer_owner, PendingIntentType::None, -1, 0);
  }

  return should_repaint;
}

auto on_timer(Core::State::AppState& state, HWND timer_owner, WPARAM timer_id) -> TimerAction {
  auto& menu_state = *state.context_menu;
  auto& interaction = menu_state.interaction;
  timer_owner = resolve_timer_owner(menu_state, timer_owner);

  if (timer_id != interaction.INTENT_TIMER_ID) {
    return {};
  }

  KillTimer(timer_owner, interaction.INTENT_TIMER_ID);
  interaction.intent_timer_id = 0;

  const auto pending_intent = interaction.pending_intent;
  const int pending_parent_index = interaction.pending_parent_index;
  interaction.pending_intent = PendingIntentType::None;
  interaction.pending_parent_index = -1;

  if (interaction.cursor_zone == Types::CursorZone::Submenu) {
    return {};
  }

  switch (pending_intent) {
    case PendingIntentType::OpenSubmenu:
    case PendingIntentType::SwitchSubmenu: {
      if (pending_parent_index < 0 ||
          pending_parent_index >= static_cast<int>(menu_state.items.size())) {
        return {};
      }
      if (pending_parent_index != interaction.hover_index) {
        return {};
      }
      if (!menu_state.items[pending_parent_index].has_submenu()) {
        return {};
      }
      return {TimerActionType::ShowSubmenu, pending_parent_index, true};
    }
    case PendingIntentType::HideSubmenu: {
      if (!menu_state.submenu_hwnd) {
        return {};
      }
      return {TimerActionType::HideSubmenu, -1, true};
    }
    case PendingIntentType::None:
    default:
      return {};
  }
}

auto get_main_highlight_index(const Core::State::AppState& state) -> int {
  const auto& menu_state = *state.context_menu;
  const auto& interaction = menu_state.interaction;

  if (!menu_state.submenu_hwnd || menu_state.submenu_parent_index < 0 ||
      menu_state.submenu_parent_index >= static_cast<int>(menu_state.items.size())) {
    return interaction.hover_index;
  }

  // 子菜单打开时：主菜单区域遵循实时 hover；离开主菜单后回显当前子菜单父项。
  if (interaction.cursor_zone == Types::CursorZone::MainMenu) {
    return interaction.hover_index;
  }

  return menu_state.submenu_parent_index;
}

}  // namespace UI::ContextMenu::Interaction
