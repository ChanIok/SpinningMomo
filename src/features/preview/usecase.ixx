module;

export module Features.Preview.UseCase;

import Core.State;
import UI.AppWindow.Events;

namespace Features::Preview::UseCase {

// 处理预览功能切换
export auto handle_preview_toggle(Core::State::AppState& state,
                                 const UI::AppWindow::Events::PreviewToggleEvent& event) -> void;

}  // namespace Features::Preview::UseCase