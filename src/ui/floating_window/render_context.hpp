#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"
#include "vendor/windows/dwrite_3.hpp"

#include "core/state/app_state.hpp"

namespace ui::floating_window::render_context {

// 初始化窗口级渲染上下文
auto initialize_render_context(core::AppState& state, HWND hwnd) -> bool;

// 清理窗口级渲染上下文
auto cleanup_render_context(core::AppState& state) -> void;

// 调整渲染目标大小
auto resize_render_context(core::AppState& state, const SIZE& new_size) -> bool;

// 更新文本格式（DPI变化时）
auto update_text_format_if_needed(core::AppState& state) -> bool;

// 测量文本宽度
auto measure_text_width(const std::wstring& text, IDWriteTextFormat* text_format,
                        IDWriteFactory7* write_factory) -> float;

// 创建具有指定字体大小的文本格式
auto create_text_format_with_size(IDWriteFactory7* write_factory, float font_size)
    -> wil::com_ptr<IDWriteTextFormat>;

// 更新所有画刷颜色
auto update_all_brush_colors(core::AppState& state) -> void;

}  // namespace ui::floating_window::render_context
