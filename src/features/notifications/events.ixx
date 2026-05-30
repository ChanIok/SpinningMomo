module;

export module Features.Notifications.Events;

import Features.Notifications.Types;

namespace Features::Notifications::Events {

export struct NotificationRequestEvent {
  Features::Notifications::Types::NotificationOptions options;
};

}  // namespace Features::Notifications::Events
