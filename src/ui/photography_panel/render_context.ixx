module;

export module UI.PhotographyPanel.RenderContext;

import Core.State;
import <windows.h>;

namespace UI::PhotographyPanel::RenderContext {

export auto ensure_render_context(Core::State::AppState& state) -> bool;
export auto resize_render_context(Core::State::AppState& state, const SIZE& new_size) -> bool;
export auto cleanup_render_context(Core::State::AppState& state) -> void;
export auto update_theme_brushes(Core::State::AppState& state) -> void;
export auto update_text_format(Core::State::AppState& state) -> bool;

}  // namespace UI::PhotographyPanel::RenderContext
