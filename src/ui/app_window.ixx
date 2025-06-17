module;

#include <iostream>
#include <windows.h>

export module UI.AppWindow;

import std;
import Common.Types;
import Core.Constants;
import Core.Events;
import Core.State;
import UI.AppWindow.Rendering;

namespace UI::AppWindow {

// ============================================================================
// 导出的结构体
// ============================================================================

// 窗口创建参数
export struct CreateParams {
  std::span<const Common::Types::RatioPreset> ratios;
  std::span<const Common::Types::ResolutionPreset> resolutions;
  const Constants::LocalizedStrings& strings;
  size_t current_ratio_index;
  size_t current_resolution_index;
  bool preview_enabled;
  bool overlay_enabled;
  bool letterbox_enabled;
};

// ============================================================================
// 窗口管理函数
// ============================================================================

// 窗口创建和销毁
export auto create_window(Core::State::AppState& state, const CreateParams& params)
    -> std::expected<void, std::wstring>;
export auto destroy_window(Core::State::AppState& state) -> void;

// 窗口显示控制
export auto show_window(Core::State::AppState& state) -> void;
export auto hide_window(Core::State::AppState& state) -> void;
export auto toggle_visibility(Core::State::AppState& state) -> void;
export auto is_window_visible(const Core::State::AppState& state) -> bool;
export auto activate_window(Core::State::AppState& state) -> void;

// 更新UI状态
export auto set_current_ratio(Core::State::AppState& state, size_t index) -> void;
export auto set_current_resolution(Core::State::AppState& state, size_t index) -> void;
export auto set_preview_enabled(Core::State::AppState& state, bool enabled) -> void;
export auto set_overlay_enabled(Core::State::AppState& state, bool enabled) -> void;
export auto set_letterbox_enabled(Core::State::AppState& state, bool enabled) -> void;

// 更新菜单项
export auto update_menu_items(Core::State::AppState& state,
                              const Constants::LocalizedStrings& strings) -> void;
export auto set_menu_items_to_show(Core::State::AppState& state,
                                   std::span<const std::wstring> items) -> void;

// ============================================================================
// 事件处理函数
// ============================================================================

// 鼠标事件处理
export auto handle_mouse_move(Core::State::AppState& state, int x, int y) -> void;
export auto handle_mouse_leave(Core::State::AppState& state) -> void;
export auto handle_left_click(Core::State::AppState& state, int x, int y) -> void;

// 热键处理
export auto register_hotkey(Core::State::AppState& state, UINT modifiers, UINT key) -> bool;
export auto unregister_hotkey(Core::State::AppState& state) -> void;
export auto handle_hotkey(Core::State::AppState& state, WPARAM hotkey_id) -> void;

// ============================================================================
// 窗口过程函数
// ============================================================================

// 窗口过程函数
export auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wParam,
                             LPARAM lParam) -> LRESULT;

// 静态窗口过程（用于注册）
export LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================================
// 内部辅助函数（不导出）
// ============================================================================

// 注册窗口类
auto register_window_class(HINSTANCE instance) -> void;

// 初始化菜单项
auto initialize_menu_items(Core::State::AppState& state, const Constants::LocalizedStrings& strings)
    -> void;

// 鼠标跟踪
auto ensure_mouse_tracking(HWND hwnd) -> void;

// 创建窗口样式和属性
auto create_window_attributes(HWND hwnd) -> void;

// 分发菜单项点击事件
auto dispatch_item_click_event(Core::State::AppState& state, const Core::State::MenuItem& item)
    -> void;

}  // namespace UI::AppWindow