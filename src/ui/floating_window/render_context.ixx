module;

export module UI.FloatingWindow.RenderContext;

import std;
import Core.State;
import <d2d1_3.h>;
import <dwrite_3.h>;
import <wil/com.h>;
import <windows.h>;

namespace UI::FloatingWindow::RenderContext {

// 初始化窗口级渲染上下文
export auto initialize_render_context(Core::State::AppState& state, HWND hwnd) -> bool;

// 清理窗口级渲染上下文
export auto cleanup_render_context(Core::State::AppState& state) -> void;

// 调整渲染目标大小
export auto resize_render_context(Core::State::AppState& state, const SIZE& new_size) -> bool;

// 更新文本格式（DPI变化时）
export auto update_text_format_if_needed(Core::State::AppState& state) -> bool;

// 测量文本宽度
export auto measure_text_width(const std::wstring& text, IDWriteTextFormat* text_format,
                               IDWriteFactory7* write_factory) -> float;

// 创建具有指定字体大小的文本格式
export auto create_text_format_with_size(IDWriteFactory7* write_factory, float font_size)
    -> wil::com_ptr<IDWriteTextFormat>;

// 更新所有画刷颜色
export auto update_all_brush_colors(Core::State::AppState& state) -> void;

}  // namespace UI::FloatingWindow::RenderContext
