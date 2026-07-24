#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"

namespace Features::Letterbox {

// 初始化和清理
auto initialize(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string>;

auto shutdown(Core::State::AppState& state) -> std::expected<void, std::string>;

// 窗口操作
auto show(Core::State::AppState& state, HWND target_window = nullptr)
    -> std::expected<void, std::string>;

auto hide(Core::State::AppState& state) -> std::expected<void, std::string>;

}  // namespace Features::Letterbox