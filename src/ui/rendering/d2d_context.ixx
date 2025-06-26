module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

export module UI.Rendering.D2DContext;

import std;
import Core.State;
import Types.UI;

namespace UI::Rendering::D2DContext {

// 内部函数声明
auto create_text_format(Core::State::AppState& state) -> std::expected<void, std::string>;
auto create_brushes(Core::State::AppState& state) -> std::expected<void, std::string>;

// 初始化Direct2D资源
export auto initialize_d2d(Core::State::AppState& state, HWND hwnd)
    -> std::expected<void, std::string>;

// 清理Direct2D资源
export auto cleanup_d2d(Core::State::AppState& state) -> void;

// 调整渲染目标大小
export auto resize_d2d(Core::State::AppState& state, const SIZE& new_size)
    -> std::expected<void, std::string>;

}  // namespace UI::Rendering::D2DContext
