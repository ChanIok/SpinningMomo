module;

#include <windows.h>

export module Features.Overlay.Utils;

import std;
import Features.Overlay.State;
import Core.State;

namespace Features::Overlay::Utils {

// 着色器代码
export auto get_vertex_shader_code() -> std::string_view;
export auto get_pixel_shader_code() -> std::string_view;

// 窗口类注册
export auto register_overlay_window_class(HINSTANCE instance) -> std::expected<void, std::string>;

// 窗口类注销
export auto unregister_overlay_window_class(HINSTANCE instance) -> void;

// 获取屏幕尺寸
export auto get_screen_dimensions() -> std::pair<int, int>;

// 计算窗口尺寸
export auto calculate_overlay_dimensions(int game_width, int game_height, int screen_width, int screen_height) 
    -> std::pair<int, int>;

// 检查游戏窗口是否需要叠加层
export auto should_use_overlay(int game_width, int game_height, int screen_width, int screen_height) -> bool;

// 获取游戏窗口尺寸
export auto get_window_dimensions(HWND hwnd) -> std::expected<std::pair<int, int>, std::string>;

// 获取游戏窗口矩形
export auto get_window_rect_safe(HWND hwnd) -> std::expected<RECT, std::string>;

// 设置窗口透明度
export auto set_window_transparency(HWND hwnd, BYTE alpha) -> std::expected<void, std::string>;

// 设置窗口层级
export auto set_window_layered_attributes(HWND hwnd) -> std::expected<void, std::string>;

// 窗口消息处理辅助函数
export auto handle_overlay_window_message(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                         Core::State::AppState& state) -> LRESULT;

// 设置全局状态指针（用于窗口过程回调）
export auto set_global_app_state(Core::State::AppState* state) -> void;

}  // namespace Features::Overlay::Utils
