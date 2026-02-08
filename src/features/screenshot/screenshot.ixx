module;

export module Features.Screenshot;

import std;
import Core.State;
import Utils.Image;
import <windows.h>;

namespace Features::Screenshot {

// 主要API：异步截图
// output_dir_override: 指定时使用该目录，否则使用 output_dir_path 或 Videos/SpinningMomo
export auto take_screenshot(
    Core::State::AppState& state, HWND target_window,
    std::function<void(bool success, const std::wstring& path)> completion_callback = nullptr,
    Utils::Image::ImageFormat format = Utils::Image::ImageFormat::PNG, float jpeg_quality = 1.0f,
    std::optional<std::filesystem::path> output_dir_override = std::nullopt)
    -> std::expected<void, std::string>;

// 系统管理函数
export auto cleanup_system(Core::State::AppState& state) -> void;

}  // namespace Features::Screenshot