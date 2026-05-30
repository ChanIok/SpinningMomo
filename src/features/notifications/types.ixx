module;

export module Features.Notifications.Types;

import std;
import Core.State;
import <d2d1_3.h>;

namespace Features::Notifications::Types {

export using NotificationActionCallback = std::function<void(Core::State::AppState&)>;

export struct NotificationAction {
  std::wstring label;
  NotificationActionCallback callback = nullptr;
};

export struct NotificationOptions {
  std::wstring title;
  std::wstring message;
  std::optional<NotificationAction> action;
  std::chrono::milliseconds duration = std::chrono::milliseconds(3000);
};

// 跨模块 export API 使用的类型（draw.ixx 等接口层不可 import State）

// 通知当前的动画/生命周期阶段
export enum class NotificationAnimState {
  Spawning,    // 正在生成，尚未创建窗口
  SlidingIn,   // 滑入动画
  Displaying,  // 正常显示
  MovingUp,    // 为新通知腾出空间而上移
  FadingOut,   // 淡出动画
  Done         // 处理完毕，待销毁
};

export struct NotificationThemeColors {
  D2D1_COLOR_F background{};
  D2D1_COLOR_F text{};
  D2D1_COLOR_F hover{};
};

// 通知创建时测量一次，后续只随 current_pos 更新 hit/draw 矩形。
export struct NotificationLayoutMetrics {
  int padding = 0;
  int content_padding = 0;
  int column_gap = 0;
  int title_height = 0;
  int title_message_gap = 0;
  int button_height = 0;
  int content_height = 0;
  int action_width = 0;
};

}  // namespace Features::Notifications::Types
