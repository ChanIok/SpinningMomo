module;

export module Features.Notifications.Constants;

import std;
import <windows.h>;

namespace Features::Notifications::Constants {

// 最大可见通知数量
export constexpr int MAX_VISIBLE_NOTIFICATIONS = 5;

// 基础尺寸 (96 DPI)
export constexpr int BASE_WINDOW_WIDTH = 350;
export constexpr int BASE_MIN_HEIGHT = 80;
export constexpr int BASE_MAX_HEIGHT = 200;
export constexpr int BASE_PADDING = 16;
export constexpr int BASE_TITLE_HEIGHT = 18;
export constexpr int BASE_TITLE_MESSAGE_GAP = 8;
export constexpr int BASE_FONT_SIZE = 13;
export constexpr int BASE_TITLE_FONT_SIZE = 14;
export constexpr int BASE_CONTENT_PADDING = 18;
export constexpr int BASE_ACTION_COLUMN_GAP = 12;
export constexpr int BASE_SPACING = 10;
export constexpr int BASE_CORNER_RADIUS = 4;
export constexpr int BASE_BUTTON_HEIGHT = 28;
export constexpr int BASE_BUTTON_WIDTH = 56;
export constexpr int BASE_BUTTON_TEXT_PADDING = 12;

// Win11 卡片描边：固定中性灰，不随背景明暗切换
export constexpr int BASE_BORDER_GRAY = 128;
export constexpr float BASE_BORDER_ALPHA = 0.45f;

// 动画计时
export constexpr auto SLIDE_DURATION = std::chrono::milliseconds(200);
export constexpr auto FADE_DURATION = std::chrono::milliseconds(200);
export constexpr auto DISPLAY_DURATION = std::chrono::milliseconds(3000);

// 动画定时器
export constexpr UINT_PTR ANIMATION_TIMER_ID = 1001;
export constexpr UINT ANIMATION_FRAME_INTERVAL = 16;  // ~60fps

// 窗口类名
export const std::wstring NOTIFICATION_WINDOW_CLASS = L"SpinningMomoNotificationHostClass";

}  // namespace Features::Notifications::Constants
