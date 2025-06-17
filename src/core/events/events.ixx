module;

#include <windows.h>

export module Core.Events;

import std;

// 应用程序事件系统
export namespace Core::Events {

// 事件类型枚举
enum class EventType {
  RatioChanged,      // 比例改变
  ResolutionChanged, // 分辨率改变
  WindowAction,      // 窗口动作
  ToggleFeature,     // 功能开关
  SystemCommand,     // 系统命令
  ConfigChanged      // 配置改变
};

// 窗口动作类型
enum class WindowAction {
  Capture,      // 截图
  Screenshot,   // 打开相册
  Reset,        // 重置窗口
  Close,        // 关闭窗口
  Exit          // 退出程序
};

// 功能开关类型
enum class FeatureType {
  Preview,      // 预览窗口
  Overlay,      // 叠加层
  Letterbox     // 黑边模式
};

// 基础事件结构
struct Event {
  EventType type;
  std::any data;
  HWND source = nullptr;
  std::chrono::steady_clock::time_point timestamp;

  Event(EventType t, std::any d, HWND src = nullptr)
      : type(t), data(std::move(d)), source(src), timestamp(std::chrono::steady_clock::now()) {}
};

// 比例改变事件数据
struct RatioChangeData {
  size_t index;
  std::wstring ratio_name;
  double ratio_value;
};

// 分辨率改变事件数据
struct ResolutionChangeData {
  size_t index;
  std::wstring resolution_name;
  std::uint64_t total_pixels;
};

// 功能开关事件数据
struct FeatureToggleData {
  FeatureType feature;
  bool enabled;
};

// 事件处理器接口
class IEventHandler {
 public:
  virtual ~IEventHandler() = default;
  virtual auto HandleEvent(const Event& event) -> void = 0;
};

// 事件分发器类
class EventDispatcher {
 public:
  using EventHandler = std::function<void(const Event&)>;

  EventDispatcher() = default;
  ~EventDispatcher() = default;

  // 禁用拷贝和移动
  EventDispatcher(const EventDispatcher&) = delete;
  auto operator=(const EventDispatcher&) -> EventDispatcher& = delete;
  EventDispatcher(EventDispatcher&&) = delete;
  auto operator=(EventDispatcher&&) -> EventDispatcher& = delete;

  // 订阅事件类型
  auto Subscribe(EventType type, EventHandler handler) -> void;
  auto Subscribe(EventType type, std::shared_ptr<IEventHandler> handler) -> void;

  // 发送同步事件
  auto SendEvent(Event event) -> void;

  // 投递异步事件
  auto PostEvent(Event event) -> void;

  // 处理队列中的事件（在消息循环中调用）
  auto ProcessEvents() -> void;

  // 清空事件队列
  auto ClearEvents() -> void;

 private:
  std::unordered_map<EventType, std::vector<EventHandler>> m_handlers;
  std::unordered_map<EventType, std::vector<std::weak_ptr<IEventHandler>>> m_interface_handlers;
  
  std::queue<Event> m_event_queue;
  std::mutex m_queue_mutex;

  // 内部处理器调用
  auto CallHandlers(const Event& event) -> void;
};

}  // namespace Core::Events 