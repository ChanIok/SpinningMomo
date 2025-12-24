module;

export module Features.Preview.Window;

import std;
import Core.State;
import Features.Preview.State;
import <windows.h>;

namespace Features::Preview::Window {

// 显示窗口
export auto show_preview_window(Core::State::AppState& state) -> void;

// 隐藏窗口
export auto hide_preview_window(Core::State::AppState& state) -> void;

// 更新DPI
export auto update_preview_window_dpi(Core::State::AppState& state, UINT new_dpi) -> void;

// 销毁窗口
export auto destroy_preview_window(Core::State::AppState& state) -> void;

// 计算窗口尺寸
export auto set_preview_window_size(Features::Preview::State::PreviewState& state,
                                    int capture_width, int capture_height) -> void;

// 初始化预览窗口系统
export auto initialize_preview_window(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string>;

}  // namespace Features::Preview::Window