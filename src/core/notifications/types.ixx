module;

export module Core.Notifications.Types;

import std;
import Core.State;

namespace Core::Notifications::Types {

export using NotificationActionCallback = std::function<void(Core::State::AppState&)>;

export struct NotificationAction {
  std::wstring label;
  NotificationActionCallback callback = nullptr;
};

export struct NotificationOptions {
  std::wstring title;
  std::wstring message;
  std::optional<NotificationAction> action;
  std::chrono::milliseconds duration = std::chrono::milliseconds(3000);
};

}  // namespace Core::Notifications::Types
