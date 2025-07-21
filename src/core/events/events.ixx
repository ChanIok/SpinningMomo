module;

export module Core.Events;

import std;
import Vendor.Windows;

namespace Core::Events {

// 事件类型枚举
export enum class EventType {
  RatioChanged,       // 比例改变
  ResolutionChanged,  // 分辨率改变
  WindowAction,       // 窗口动作
  WindowSelected,     // 窗口选择
  ToggleFeature,      // 功能开关
  SystemCommand,      // 系统命令
  ConfigChanged,      // 配置改变
  WebViewResponse,    // WebView响应
  DpiChanged          // DPI改变
};

// 窗口动作类型
export enum class WindowAction {
  Capture,      // 截图
  Screenshots,  // 打开相册
  Reset,        // 重置窗口
  Hide,         // 关闭窗口
  Exit          // 退出程序
};

// 功能开关类型
export enum class FeatureType {
  Preview,   // 预览窗口
  Overlay,   // 叠加层
  Letterbox  // 黑边模式
};

// 基础事件结构
export struct Event {
  EventType type;
  std::any data;
  Vendor::Windows::HWND source = nullptr;
  std::chrono::steady_clock::time_point timestamp;

  Event(EventType t, std::any d, Vendor::Windows::HWND src = nullptr)
      : type(t), data(std::move(d)), source(src), timestamp(std::chrono::steady_clock::now()) {}
};

// 比例改变事件数据
export struct RatioChangeData {
  size_t index;
  std::wstring ratio_name;
  double ratio_value;
};

// 分辨率改变事件数据
export struct ResolutionChangeData {
  size_t index;
  std::wstring resolution_name;
  std::uint64_t total_pixels;
};

// 窗口选择事件数据 - 新增
export struct WindowSelectionData {
  Vendor::Windows::HWND window_handle;
  std::wstring window_title;
};

// 功能开关事件数据
export struct FeatureToggleData {
  FeatureType feature;
  bool enabled;
};

// DPI改变事件数据
export struct DpiChangeData {
  Vendor::Windows::UINT new_dpi;
  Vendor::Windows::SIZE window_size;
};

// 事件处理器函数类型
export using EventHandler = std::function<void(const Event&)>;

// 事件总线（纯数据结构）
export struct EventBus {
  std::unordered_map<EventType, std::vector<EventHandler>> handlers;
  std::queue<Event> event_queue;
  std::mutex queue_mutex;
};

// 操作EventBus的自由函数
export auto subscribe(EventBus& bus, EventType type, EventHandler handler) -> void;

// 发送同步事件
export auto send_event(EventBus& bus, const Event& event) -> void;

// 投递异步事件
export auto post_event(EventBus& bus, Event event) -> void;

// 处理队列中的事件（在消息循环中调用）
export auto process_events(EventBus& bus) -> void;

// 清空事件队列
export auto clear_events(EventBus& bus) -> void;

}  // namespace Core::Events