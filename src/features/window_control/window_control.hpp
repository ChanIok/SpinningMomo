#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "features/window_control/types.hpp"

namespace features::window_control {

auto get_window_title(HWND hwnd) -> std::expected<std::wstring, std::string>;

auto find_target_window(const std::wstring& configured_title) -> std::expected<HWND, std::string>;

auto get_visible_windows() -> std::vector<WindowInfo>;

auto toggle_window_border(HWND hwnd) -> std::expected<bool, std::string>;

auto calculate_resolution_by_area(double ratio, std::uint64_t total_area) -> Resolution;
auto calculate_resolution_by_screen(double ratio, int screen_width, int screen_height)
    -> Resolution;
auto calculate_resolution_from_preset(double ratio, const ResolutionPresetInput& resolution_preset,
                                      const ResolutionCalculationOptions& options) -> Resolution;

auto apply_window_transform(core::AppState& state, HWND target_window, const Resolution& resolution,
                            const TransformOptions& options = {})
    -> std::expected<void, std::string>;

auto reset_window_to_screen(core::AppState& state, HWND target_window,
                            const TransformOptions& options = {})
    -> std::expected<void, std::string>;

auto resize_and_center_window(core::AppState& state, HWND hwnd, int width, int height,
                              bool activate) -> std::expected<void, std::string>;

auto start_center_lock_monitor(core::AppState& state) -> std::expected<void, std::string>;

auto stop_center_lock_monitor(core::AppState& state) -> void;

}  // namespace features::window_control
