module;

export module Core.Notifications.Events;

import Core.Notifications.Types;

namespace Core::Notifications::Events {

export struct NotificationRequestEvent {
  Core::Notifications::Types::NotificationOptions options;
};

}  // namespace Core::Notifications::Events
