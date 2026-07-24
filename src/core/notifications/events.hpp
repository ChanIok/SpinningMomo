#pragma once

#include "core/notifications/types.hpp"

namespace Core::Notifications::Events {

struct NotificationRequestEvent {
  Core::Notifications::Types::NotificationOptions options;
};

}  // namespace Core::Notifications::Events
