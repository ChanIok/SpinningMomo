module;

#include <windows.h>

export module Features.Preview.Window;

import std;
import Types.Preview;
import Core.State;

export namespace Features::Preview::Window {

// 创建预览窗口
auto create_window(HINSTANCE instance, HWND parent, Core::State::AppState* state)
    -> std::expected<HWND, std::string>;

// 初始化预览系统
auto initialize_preview(Core::State::AppState& state, HINSTANCE instance, HWND parent)
    -> std::expected<void, std::string>;

// 开始捕获并显示预览
auto start_preview(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string>;

// 停止预览并隐藏窗口
auto stop_preview(Core::State::AppState& state) -> void;

// 窗口显示/隐藏控制
auto show_preview_window(Core::State::AppState& state) -> void;
auto hide_preview_window(Core::State::AppState& state) -> void;
auto is_preview_window_visible(const Core::State::AppState& state) -> bool;

// 窗口操作
auto resize_preview_window(Core::State::AppState& state, int width, int height) -> void;
auto move_preview_window(Core::State::AppState& state, int x, int y) -> void;

// DPI 处理
auto update_preview_dpi(Core::State::AppState& state, UINT new_dpi) -> void;

// 清理资源
auto cleanup_preview(Core::State::AppState& state) -> void;

// 获取预览窗口句柄
auto get_preview_window_handle(const Core::State::AppState& state) -> HWND;

// 检查预览是否正在运行
auto is_preview_running(const Core::State::AppState& state) -> bool;

}  // namespace Features::Preview::Window