#pragma once
#include "notification_window.hpp"
#include <queue>
#include <memory>

class NotificationManager {
public:
    static constexpr int MAX_VISIBLE_NOTIFICATIONS = 3;

    NotificationManager(HINSTANCE hInstance);
    
    void ShowNotification(const std::wstring& title, 
                         const std::wstring& message,
                         NotificationWindow::NotificationType type = NotificationWindow::NotificationType::Info);
    
    void ClearAll();

private:
    HINSTANCE m_hInstance;
    std::queue<std::unique_ptr<NotificationWindow>> m_notificationQueue;
    int m_activeNotifications = 0;

    void TriggerOldestFadeOut();
    void NotifyWindowsToMoveUp(int newWindowHeight);
    void RemoveFromQueue(NotificationWindow* notification);
}; 