module;

export module Core.Actions;

import std;
import Core.State;

namespace Core::Actions {

export namespace Payloads {
  struct SetCurrentRatio {
    size_t index;
  };
  struct SetCurrentResolution {
    size_t index;
  };
  struct TogglePreview {
    bool enabled;
  };
  struct ToggleOverlay {
    bool enabled;
  };
  struct ToggleLetterbox {
    bool enabled;
  };
  struct ResetWindowState {};
  struct UpdateHoverIndex {
    int index;
  };
}

export using ActionPayload =
    std::variant<Payloads::SetCurrentRatio, Payloads::SetCurrentResolution,
                 Payloads::TogglePreview, Payloads::ToggleOverlay, Payloads::ToggleLetterbox,
                 Payloads::ResetWindowState, Payloads::UpdateHoverIndex>;

export struct Action {
  ActionPayload payload;
  std::chrono::steady_clock::time_point timestamp;

  template <typename T>
  Action(T&& p) : payload(std::forward<T>(p)), timestamp(std::chrono::steady_clock::now()) {}
};

export auto dispatch_action(Core::State::AppState& state, const Action& action) -> void;

export auto trigger_ui_update(Core::State::AppState& state) -> void;

}  // namespace Core::Actions 