#include "notification_manager.hpp"

NotificationManager::NotificationManager(HINSTANCE hInstance) 
    : m_hInstance(hInstance) {
}

void NotificationManager::ShowNotification(const std::wstring& title, 
    const std::wstring& message, NotificationWindow::NotificationType type) {
    
    auto notification = std::make_unique<NotificationWindow>(m_hInstance);
    
    // 设置生命周期结束回调
    notification->SetOnLifecycleEnd([this](NotificationWindow* sender) {
        m_activeNotifications--;
        RemoveFromQueue(sender);
    });

    // 如果当前显示数量达到上限，触发最老的通知淡出
    if (m_activeNotifications >= MAX_VISIBLE_NOTIFICATIONS) {
        TriggerOldestFadeOut();
    }

    // 计算新通知的高度
    int newHeight = notification->CalculateHeight(message);

    // 通知所有活动窗口上移
    NotifyWindowsToMoveUp(newHeight);

    // 显示新通知
    notification->Show(title, message, type);
    m_activeNotifications++;
    
    // 加入队列
    m_notificationQueue.push(std::move(notification));
}

void NotificationManager::TriggerOldestFadeOut() {
    if (!m_notificationQueue.empty()) {
        m_notificationQueue.front()->StartFadeOut();
    }
}

void NotificationManager::NotifyWindowsToMoveUp(int newWindowHeight) {
    // 遍历队列中的所有活动通知
    std::queue<std::unique_ptr<NotificationWindow>> tempQueue;
    
    while (!m_notificationQueue.empty()) {
        auto& currentWindow = m_notificationQueue.front();
        if (currentWindow->IsActive() && !currentWindow->IsFadingOut()) {
            // 使用新通知的高度来计算移动距离
            currentWindow->StartMoveUpAnimation(newWindowHeight);
        }
        tempQueue.push(std::move(m_notificationQueue.front()));
        m_notificationQueue.pop();
    }
    m_notificationQueue = std::move(tempQueue);
}

void NotificationManager::RemoveFromQueue(NotificationWindow* notification) {
    std::queue<std::unique_ptr<NotificationWindow>> tempQueue;
    while (!m_notificationQueue.empty()) {
        if (m_notificationQueue.front().get() != notification) {
            tempQueue.push(std::move(m_notificationQueue.front()));
        }
        m_notificationQueue.pop();
    }
    m_notificationQueue = std::move(tempQueue);
}

void NotificationManager::ClearAll() {
    while (!m_notificationQueue.empty()) {
        if (m_notificationQueue.front()->IsActive()) {
            m_notificationQueue.front()->StartFadeOut();
        }
        m_notificationQueue.pop();
    }
} 