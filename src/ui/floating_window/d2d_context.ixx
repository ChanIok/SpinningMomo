module;

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <windows.h>

export module UI.FloatingWindow.D2DContext;

import std;
import Core.State;

namespace UI::FloatingWindow::D2DContext {

// 初始化Direct2D资源
export auto initialize_d2d(Core::State::AppState& state, HWND hwnd) -> bool;

// 清理Direct2D资源
export auto cleanup_d2d(Core::State::AppState& state) -> void;

// 调整渲染目标大小
export auto resize_d2d(Core::State::AppState& state, const SIZE& new_size) -> bool;

// 更新文本格式（DPI变化时）
export auto update_text_format_if_needed(Core::State::AppState& state) -> bool;

// 测量文本宽度
export auto measure_text_width(const std::wstring& text, IDWriteTextFormat* text_format,
                               IDWriteFactory7* write_factory) -> float;

// 创建具有指定字体大小的文本格式
export auto create_text_format_with_size(IDWriteFactory7* write_factory, float font_size)
    -> IDWriteTextFormat*;

// 更新所有画刷颜色
export auto update_all_brush_colors(Core::State::AppState& state) -> void;

}  // namespace UI::FloatingWindow::D2DContext
