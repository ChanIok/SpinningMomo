module;

#include <windows.h>  // 必须放在最前面  
#include <shellapi.h>

#include <filesystem>

export module Features.Screenshot.Folder;

import std;
import Features.WindowControl;
import Features.Settings.State;
import Core.State;
import Utils.Logger;
import Utils.Path;

namespace Features::Screenshot::Folder {

// 从游戏窗口发现截图路径
auto discover_game_path(HWND game_window) -> std::expected<std::filesystem::path, std::string> {
  if (!game_window) {
    return std::unexpected("Invalid game window handle");
  }

  // 获取进程ID
  DWORD process_id;
  GetWindowThreadProcessId(game_window, &process_id);
  if (process_id == 0) {
    return std::unexpected("Failed to get process ID");
  }

  // 打开进程
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (!process) {
    return std::unexpected("Failed to open process");
  }

  // RAII 管理进程句柄
  auto process_guard =
      std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)>(process, &CloseHandle);

  // 获取进程路径
  wchar_t process_path[MAX_PATH];
  DWORD size = MAX_PATH;
  if (!QueryFullProcessImageNameW(process, 0, process_path, &size)) {
    return std::unexpected("Failed to get process image name");
  }

  // 构建游戏截图路径
  std::filesystem::path path = process_path;
  auto bin_pos = path.wstring().find(L"\\Binaries\\Win64");
  if (bin_pos != std::wstring::npos) {
    auto game_root = path.wstring().substr(0, bin_pos);
    auto screenshot_path = std::filesystem::path(game_root) / "ScreenShot";

    if (std::filesystem::exists(screenshot_path)) {
      Logger().debug("Discovered game screenshot path: {}", screenshot_path.string());
      return screenshot_path;
    }
  }

  return std::unexpected("Game screenshot directory not found");
}

// 解析截图目录路径（核心逻辑）
auto resolve_path(const Features::Settings::State::SettingsState& settings, HWND game_window = nullptr)
    -> std::expected<std::filesystem::path, std::string> {
  // 1. 检查配置中的路径（暂时硬编码为空）
  // TODO: 等待settings设计完成后，从settings中读取截图路径
  std::string screenshot_path_config = ""; // 硬编码为空
  if (!screenshot_path_config.empty()) {
    std::filesystem::path config_path = screenshot_path_config;
    if (std::filesystem::exists(config_path)) {
      Logger().debug("Using configured screenshot path: {}", config_path.string());
      return config_path;
    } else {
      Logger().warn("Configured screenshot path does not exist: {}", config_path.string());
    }
  }

  // 2. 尝试从游戏窗口发现路径
  if (game_window) {
    if (auto game_path = discover_game_path(game_window)) {
      Logger().debug("Discovered game screenshot path from window");
      return game_path.value();
    } else {
      Logger().debug("Failed to discover game path: {}", game_path.error());
    }
  }

  // 3. 尝试通过配置的窗口标题查找游戏窗口（暂时硬编码为空）
  // TODO: 等待settings设计完成后，从settings中读取窗口标题
  std::wstring window_title_config = L""; // 硬编码为空
  if (!window_title_config.empty()) {
    auto target_window = Features::WindowControl::find_target_window(window_title_config);
    if (target_window) {
      if (auto game_path = discover_game_path(target_window.value())) {
        Logger().debug("Discovered game screenshot path from configured window");
        return game_path.value();
      }
    }
  }

  // 4. 回退到程序截图目录
  auto exe_dir_result = Utils::Path::GetExecutableDirectory();
  if (!exe_dir_result) {
    return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
  }

  auto screenshots_dir = exe_dir_result.value() / "Screenshots";
  auto ensure_result = Utils::Path::EnsureDirectoryExists(screenshots_dir);
  if (!ensure_result) {
    return std::unexpected("Failed to create screenshots directory: " + ensure_result.error());
  }

  Logger().debug("Using default screenshot path: {}", screenshots_dir.string());
  return screenshots_dir;
}

// 打开截图文件夹（对外接口）
export auto open_folder(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 解析路径
  auto path_result = resolve_path(state.settings);
  if (!path_result) {
    return std::unexpected("Failed to resolve screenshot path: " + path_result.error());
  }

  auto path = path_result.value();

  // 使用 ShellExecuteW 打开文件夹
  auto result = ShellExecuteW(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  if (reinterpret_cast<intptr_t>(result) <= 32) {
    return std::unexpected("Failed to open screenshot folder: error code " +
                           std::to_string(reinterpret_cast<intptr_t>(result)));
  }

  Logger().info("Opened screenshot folder: {}", path.string());
  return {};
}

}  // namespace Features::Screenshot::Folder