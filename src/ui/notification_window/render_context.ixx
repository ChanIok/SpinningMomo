module;

export module UI.NotificationWindow.RenderContext;

import Core.State;
import <windows.h>;

namespace UI::NotificationWindow::RenderContext {

export auto ensure_render_context(Core::State::AppState& state) -> bool;
export auto cleanup_render_context(Core::State::AppState& state) -> void;
export auto resize_render_context(Core::State::AppState& state, const SIZE& new_size) -> bool;

}  // namespace UI::NotificationWindow::RenderContext
