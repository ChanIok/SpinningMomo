#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include <functional>

class NotificationWindow {
public:
    // 基础尺寸（96 DPI下的值）
    static constexpr int BASE_WINDOW_WIDTH = 300;
    static constexpr int BASE_WINDOW_HEIGHT = 80;
    static constexpr int BASE_LINE_HEIGHT = 20;     // 单行文本高度
    static constexpr int BASE_MIN_HEIGHT = 80;      // 最小窗口高度
    static constexpr int BASE_MAX_HEIGHT = 200;     // 最大窗口高度
    static constexpr int BASE_PADDING = 12;
    static constexpr int BASE_TITLE_HEIGHT = 24;
    static constexpr int BASE_FONT_SIZE = 14;
    static constexpr int BASE_TITLE_FONT_SIZE = 14;
    
    // UI常量
    static constexpr int BASE_CLOSE_SIZE = 12;       // 关闭按钮尺寸
    static constexpr int BASE_CLOSE_PADDING = 12;    // 关闭按钮边距
    static constexpr int BASE_CONTENT_PADDING = 16;  // 内容左边距
    static constexpr int BASE_SPACING = 10;          // 通知间距

    enum class NotificationType {
        Info,       // 用于普通信息和成功信息
        Error       // 用于错误信息
    };

    NotificationWindow(HINSTANCE hInstance);
    ~NotificationWindow();

    // 核心功能
    void Show(const std::wstring& title, const std::wstring& message, NotificationType type);
    void StartFadeOut();
    void StartMoveUpAnimation(int nextWindowHeight);
    bool IsActive() const { return m_isActive; }
    bool IsFadingOut() const { return m_animState == AnimationState::FADING_OUT; }
    int GetHeight() const { return m_windowHeight; }
    int CalculateHeight(const std::wstring& message) const {
        return CalculateWindowHeight(message);
    }
    void SetOnLifecycleEnd(std::function<void(NotificationWindow*)> callback) {
        m_onLifecycleEnd = std::move(callback);
    }

private:
    // 动画状态
    enum class AnimationState {
        NONE,
        SLIDING_IN,    // 新通知滑入
        DISPLAYING,    // 显示中
        MOVING_UP,     // 向上移动
        FADING_OUT    // 淡出
    };

    struct NotificationItem {
        std::wstring title;
        std::wstring message;
        NotificationType type = NotificationType::Info;
        ULONGLONG timestamp = 0;
        float opacity = 0.0f;
    };

    // 窗口相关
    HWND m_hwnd = NULL;
    HINSTANCE m_hInstance = NULL;
    std::unique_ptr<NotificationItem> m_currentNotification;
    bool m_isActive = false;
    std::function<void(NotificationWindow*)> m_onLifecycleEnd;
    
    // DPI相关
    int m_dpi = 96;
    int m_windowWidth;
    int m_windowHeight;
    int m_padding;
    int m_titleHeight;
    int m_fontSize;
    int m_titleFontSize;
    int m_closeSize;          // 关闭按钮尺寸
    int m_closePadding;       // 关闭按钮边距
    int m_contentPadding;     // 内容边距

    // UI状态
    bool m_isCloseHovered = false;  // 关闭按钮悬停状态
    bool m_isWindowHovered = false;  // 窗口悬停状态
    ULONGLONG m_pausedTime = 0;      // 暂停时的时间点
    ULONGLONG m_totalPausedTime = 0; // 累计暂停时间

    // 动画相关
    static constexpr int ANIMATION_DURATION = 200;   // 基础动画时长
    static constexpr int SLIDE_DURATION = 200;       // 滑动动画时长
    static constexpr int DISPLAY_DURATION = 3000;    // 显示持续时间
    UINT_PTR m_animationTimer = 0;
    AnimationState m_animState = AnimationState::NONE;
    float m_animProgress = 0.0f;
    POINT m_startPos = {0, 0};
    POINT m_targetPos = {0, 0};

    // 颜色定义
    const COLORREF BG_COLOR = RGB(255, 255, 255);      // 背景色
    const COLORREF INFO_COLOR = RGB(0, 120, 212);      // Windows 11风格的蓝色
    const COLORREF ERROR_COLOR = RGB(235, 69, 51);     // 柔和的红色
    const COLORREF TEXT_COLOR = RGB(96, 96, 96);       // 文本色
    const COLORREF TITLE_COLOR = RGB(38, 38, 38);      // 标题色
    const COLORREF CLOSE_NORMAL_COLOR = RGB(128, 128, 128);  // 关闭按钮颜色
    const COLORREF CLOSE_HOVER_COLOR = RGB(51, 51, 51);      // 悬停颜色

    // 私有方法
    void RegisterWindowClass();
    bool CreateNotificationWindow();
    void UpdateDpiDependentResources();
    void StartAnimation();
    void StopAnimation();
    void UpdateWindowPosition();
    float EaseOutCubic(float t) const;
    void OnAnimationComplete();
    void OnDisplayTimeout();
    POINT CalculateNextPosition() const;

    // 绘制相关
    void DrawCloseButton(HDC hdc, const RECT& rect);
    bool IsPointInCloseButton(int x, int y) const;
    std::wstring GetTypeString(NotificationType type) const;

    // 消息处理
    static LRESULT CALLBACK NotificationWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HDC hdc);
    void OnTimer(UINT_PTR timerId);
    void OnMouseMove(int x, int y);
    void OnMouseLeave();

    int CalculateWindowHeight(const std::wstring& message) const;
}; 