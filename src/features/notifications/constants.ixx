module;

#include <windows.h>
#include <chrono>

export module Features.Notifications.Constants;

export namespace Features::Notifications::Constants {

// 最大可见通知数量
constexpr int MAX_VISIBLE_NOTIFICATIONS = 5;

// 基础尺寸 (96 DPI)
constexpr int BASE_WINDOW_WIDTH = 300;
constexpr int BASE_MIN_HEIGHT = 80;
constexpr int BASE_MAX_HEIGHT = 200;
constexpr int BASE_PADDING = 12;
constexpr int BASE_TITLE_HEIGHT = 24;
constexpr int BASE_FONT_SIZE = 14;
constexpr int BASE_TITLE_FONT_SIZE = 14;
constexpr int BASE_CLOSE_SIZE = 12;
constexpr int BASE_CLOSE_PADDING = 12;
constexpr int BASE_CONTENT_PADDING = 16;
constexpr int BASE_SPACING = 10;

// 动画计时
constexpr auto SLIDE_DURATION = std::chrono::milliseconds(200);
constexpr auto FADE_DURATION = std::chrono::milliseconds(200);
constexpr auto DISPLAY_DURATION = std::chrono::milliseconds(3000);

// 动画定时器
constexpr UINT_PTR ANIMATION_TIMER_ID = 1001;
constexpr UINT ANIMATION_FRAME_INTERVAL = 16;  // ~60fps

// 颜色
const COLORREF BG_COLOR = RGB(255, 255, 255);
const COLORREF TEXT_COLOR = RGB(96, 96, 96);
const COLORREF TITLE_COLOR = RGB(38, 38, 38);
const COLORREF CLOSE_NORMAL_COLOR = RGB(128, 128, 128);
const COLORREF CLOSE_HOVER_COLOR = RGB(51, 51, 51);

// 窗口类名
const std::wstring NOTIFICATION_WINDOW_CLASS = L"SpinningMomoNotificationClass";

}  // namespace Features::Notifications::Constants