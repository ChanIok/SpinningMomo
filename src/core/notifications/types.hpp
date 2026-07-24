#pragma once

#include "core/state/app_state.hpp"

namespace Core::Notifications::Types {

using NotificationActionCallback = std::function<void(Core::State::AppState&)>;

struct NotificationAction {
  std::wstring label;
  NotificationActionCallback callback = nullptr;
};

struct NotificationOptions {
  std::wstring title;
  std::wstring message;
  std::optional<NotificationAction> action;
  std::chrono::milliseconds duration = std::chrono::milliseconds(3000);
};

}  // namespace Core::Notifications::Types
