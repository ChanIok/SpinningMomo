#pragma once

#include "vendor/std.hpp"

#include "core/notifications/types.hpp"

namespace core::notifications::events {

struct NotificationRequestEvent {
  core::notifications::NotificationOptions options;
};

}  // namespace core::notifications::events
