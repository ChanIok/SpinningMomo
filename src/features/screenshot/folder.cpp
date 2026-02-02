module;

#include <comdef.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <windows.h>  // 必须放在最前面

module Features.Screenshot.Folder;

import std;
import Core.State;
import Core.WebView.State;
import Features.WindowControl;
import Features.Settings.State;
import Utils.Logger;
import Utils.Path;
import Utils.String;

// 定义COM资源的删除器
struct ComDeleter {
  void operator()(void*) const { CoUninitialize(); }
};

// 定义COM对象的删除器
template <typename T>
struct ComPtrDeleter {
  void operator()(T* ptr) const {
    if (ptr) ptr->Release();
  }
};

// 定义CoTaskMemFree的删除器
struct CoTaskMemDeleter {
  void operator()(wchar_t* ptr) const {
    if (ptr) CoTaskMemFree(ptr);
  }
};

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
auto resolve_path(const Core::State::AppState& state)
    -> std::expected<std::filesystem::path, std::string> {
  // 1. 检查配置中的路径
  std::string screenshot_path_config = state.settings->raw.features.screenshot.screenshot_dir_path;
  if (!screenshot_path_config.empty()) {
    std::filesystem::path config_path = screenshot_path_config;
    if (std::filesystem::exists(config_path)) {
      Logger().debug("Using configured screenshot path: {}", config_path.string());
      return config_path;
    } else {
      Logger().warn("Configured screenshot path does not exist: {}", config_path.string());
    }
  }

  // 2. 尝试通过配置的窗口标题查找游戏窗口
  std::wstring window_title_config =
      Utils::String::FromUtf8(state.settings->raw.window.target_title);
  if (!window_title_config.empty()) {
    auto target_window = Features::WindowControl::find_target_window(window_title_config);
    if (target_window) {
      if (auto game_path = discover_game_path(target_window.value())) {
        Logger().debug("Discovered game screenshot path from configured window");
        return game_path.value();
      }
    }
  }

  // 3. 回退到程序截图目录
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
auto open_folder(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 解析路径
  auto path_result = resolve_path(state);
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

// 选择截图文件夹（对外接口）
auto select_folder(Core::State::AppState& state)
    -> std::expected<std::filesystem::path, std::string> {
  // 初始化COM
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    return std::unexpected("Failed to initialize COM library");
  }

  // RAII管理COM库的释放
  auto com_guard = std::unique_ptr<void, ComDeleter>(nullptr);

  // 创建文件对话框实例
  IFileDialog* pFileDialog = nullptr;
  hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileDialog,
                        reinterpret_cast<void**>(&pFileDialog));
  if (FAILED(hr)) {
    return std::unexpected("Failed to create file dialog instance");
  }

  // RAII管理COM对象的释放
  auto dialog_guard = std::unique_ptr<IFileDialog, ComPtrDeleter<IFileDialog>>(pFileDialog);

  // 设置选项，只允许选择文件夹
  DWORD dwOptions;
  hr = pFileDialog->GetOptions(&dwOptions);
  if (SUCCEEDED(hr)) {
    hr = pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);
  }
  if (FAILED(hr)) {
    return std::unexpected("Failed to set dialog options");
  }

  // 设置标题
  pFileDialog->SetTitle(L"选择截图目录");

  // 获取webview窗口句柄
  HWND webview_hwnd = state.webview->window.webview_hwnd;

  // 显示对话框
  hr = pFileDialog->Show(webview_hwnd);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return std::unexpected("User cancelled the operation");
  }
  if (FAILED(hr)) {
    return std::unexpected("Failed to show dialog");
  }

  // 获取结果
  IShellItem* pItem = nullptr;
  hr = pFileDialog->GetResult(&pItem);
  if (FAILED(hr)) {
    return std::unexpected("Failed to get dialog result");
  }

  // RAII管理ShellItem的释放
  auto item_guard = std::unique_ptr<IShellItem, ComPtrDeleter<IShellItem>>(pItem);

  // 获取路径
  PWSTR pszFilePath = nullptr;
  hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
  if (FAILED(hr)) {
    return std::unexpected("Failed to get selected path");
  }

  // RAII管理字符串的释放
  auto string_guard = std::unique_ptr<wchar_t, CoTaskMemDeleter>(pszFilePath);

  // 转换为std::filesystem::path并返回
  std::filesystem::path result(pszFilePath);
  Logger().info("User selected folder: {}", result.string());
  return result;
}

}  // namespace Features::Screenshot::Folder