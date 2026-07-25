#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::notifications {

using NotificationActionCallback = std::function<void(core::AppState&)>;

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

}  // namespace core::notifications
