module;

module Utils.System;

import std;
import <windows.h>;
import Vendor.ShellApi;
import Vendor.Windows;
import Utils.String;

namespace Utils::System {

// Windows 约定：Preferred DropEffect = 1 表示“复制”，
// 这样其他程序在粘贴这些文件时会按“复制文件”来理解，而不是“移动文件”。
constexpr DWORD kClipboardDropEffectCopy = 1;

// 获取当前 Windows 系统版本信息
[[nodiscard]] auto get_windows_version() noexcept
    -> std::expected<WindowsVersionInfo, std::string> {
  RTL_OSVERSIONINFOW osInfo = {sizeof(RTL_OSVERSIONINFOW)};
  HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");

  if (!hNtDll) [[unlikely]]
    return std::unexpected("Failed to get module handle for ntdll.dll");

  typedef LONG(NTAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
  RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtDll, "RtlGetVersion");

  if (!RtlGetVersion) [[unlikely]]
    return std::unexpected("Failed to get RtlGetVersion function address");

  RtlGetVersion(&osInfo);
  return WindowsVersionInfo{osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber,
                            osInfo.dwPlatformId};
}

// 根据 WindowsVersionInfo 获取系统名称
[[nodiscard]] auto get_windows_name(const WindowsVersionInfo& version) noexcept -> std::string {
  if (version.major_version == 10) {
    if (version.build_number >= 22000) {
      return "Windows 11";
    } else {
      return "Windows 10";
    }
  } else if (version.major_version == 6 && version.minor_version == 1) {
    return "Windows 7";
  } else if (version.major_version == 6 && version.minor_version == 2) {
    return "Windows 8";
  } else if (version.major_version == 6 && version.minor_version == 3) {
    return "Windows 8.1";
  } else {
    return "Windows";
  }
}

// 检测当前进程是否以管理员权限运行
[[nodiscard]] auto is_process_elevated() noexcept -> bool {
  // 使用静态变量缓存结果，避免重复检测
  static bool result = []() {
    BYTE admin_sid[SECURITY_MAX_SID_SIZE]{};
    DWORD sid_size = sizeof(admin_sid);

    // 创建管理员组的 SID
    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, &admin_sid, &sid_size)) {
      return false;
    }

    BOOL is_admin = FALSE;
    // 检查当前进程令牌是否属于管理员组
    if (!CheckTokenMembership(nullptr, admin_sid, &is_admin)) {
      return false;
    }

    return is_admin != FALSE;
  }();

  return result;
}

// 以管理员权限重启当前应用程序
[[nodiscard]] auto restart_as_elevated(const wchar_t* arguments) noexcept -> bool {
  // 获取当前可执行文件路径
  wchar_t exe_path[MAX_PATH]{};
  if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0) {
    return false;
  }

  // 使用 ShellExecuteEx 请求提升权限
  Vendor::ShellApi::SHELLEXECUTEINFOW exec_info{
      .cbSize = sizeof(exec_info),
      .fMask = Vendor::ShellApi::kSEE_MASK_NOASYNC,  // 同步执行
      .lpVerb = L"runas",                            // 请求提升权限
      .lpFile = exe_path,                            // 要执行的文件
      .lpParameters = arguments,                     // 命令行参数
      .nShow = Vendor::ShellApi::kSW_SHOWNORMAL      // 显示窗口
  };

  return Vendor::ShellApi::ShellExecuteExW(&exec_info) != FALSE;
}

auto open_directory(const std::filesystem::path& path) -> std::expected<void, std::string> {
  if (path.empty()) {
    return std::unexpected("Directory path is empty");
  }

  try {
    if (!std::filesystem::exists(path)) {
      return std::unexpected("Directory does not exist: " + path.string());
    }

    if (!std::filesystem::is_directory(path)) {
      return std::unexpected("Path is not a directory: " + path.string());
    }
  } catch (const std::exception& e) {
    return std::unexpected("Failed to validate directory path: " + std::string(e.what()));
  }

  std::wstring wpath = std::filesystem::path(path).make_preferred().wstring();
  Vendor::ShellApi::SHELLEXECUTEINFOW exec_info{
      .cbSize = sizeof(exec_info),
      .fMask = Vendor::ShellApi::kSEE_MASK_NOASYNC,
      .lpVerb = L"open",
      .lpFile = wpath.c_str(),
      .nShow = Vendor::ShellApi::kSW_SHOWNORMAL,
  };

  if (!Vendor::ShellApi::ShellExecuteExW(&exec_info)) {
    return std::unexpected("Failed to open directory, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  return {};
}

auto open_file_with_default_app(const std::filesystem::path& path)
    -> std::expected<void, std::string> {
  if (path.empty()) {
    return std::unexpected("File path is empty");
  }

  try {
    if (!std::filesystem::exists(path)) {
      return std::unexpected("File does not exist: " + path.string());
    }

    if (!std::filesystem::is_regular_file(path)) {
      return std::unexpected("Path is not a file: " + path.string());
    }
  } catch (const std::exception& e) {
    return std::unexpected("Failed to validate file path: " + std::string(e.what()));
  }

  std::wstring wpath = std::filesystem::path(path).make_preferred().wstring();
  Vendor::ShellApi::SHELLEXECUTEINFOW exec_info{
      .cbSize = sizeof(exec_info),
      .fMask = Vendor::ShellApi::kSEE_MASK_NOASYNC,
      .lpVerb = L"open",
      .lpFile = wpath.c_str(),
      .nShow = Vendor::ShellApi::kSW_SHOWNORMAL,
  };

  if (!Vendor::ShellApi::ShellExecuteExW(&exec_info)) {
    return std::unexpected("Failed to open file with default app, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  return {};
}

auto reveal_file_in_explorer(const std::filesystem::path& path)
    -> std::expected<void, std::string> {
  if (path.empty()) {
    return std::unexpected("File path is empty");
  }

  try {
    if (!std::filesystem::exists(path)) {
      return std::unexpected("File does not exist: " + path.string());
    }
  } catch (const std::exception& e) {
    return std::unexpected("Failed to validate file path: " + std::string(e.what()));
  }

  std::wstring wpath = std::filesystem::path(path).make_preferred().wstring();
  std::wstring params = std::format(LR"(/select,"{}")", wpath);
  Vendor::ShellApi::SHELLEXECUTEINFOW exec_info{
      .cbSize = sizeof(exec_info),
      .fMask = Vendor::ShellApi::kSEE_MASK_NOASYNC,
      .lpVerb = L"open",
      .lpFile = L"explorer.exe",
      .lpParameters = params.c_str(),
      .nShow = Vendor::ShellApi::kSW_SHOWNORMAL,
  };

  if (!Vendor::ShellApi::ShellExecuteExW(&exec_info)) {
    return std::unexpected("Failed to reveal file in explorer, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  return {};
}

auto copy_files_to_clipboard(const std::vector<std::filesystem::path>& paths)
    -> std::expected<void, std::string> {
  // 这里复制的是“文件对象”到系统剪贴板，不是复制路径文本。
  // 目标效果要和资源管理器里 Ctrl+C 文件尽量一致。
  if (paths.empty()) {
    return std::unexpected("No file paths to copy");
  }

  std::vector<std::wstring> normalized_paths;
  normalized_paths.reserve(paths.size());

  size_t total_chars = 0;
  for (const auto& path : paths) {
    if (path.empty()) {
      continue;
    }

    auto normalized_path = std::filesystem::path(path).make_preferred().wstring();
    if (normalized_path.empty()) {
      continue;
    }

    total_chars += normalized_path.size() + 1;
    normalized_paths.push_back(std::move(normalized_path));
  }

  if (normalized_paths.empty()) {
    return std::unexpected("No valid file paths to copy");
  }

  auto free_global = [](HGLOBAL handle) {
    if (handle != nullptr) {
      GlobalFree(handle);
    }
  };

  // CF_HDROP 需要一块连续内存：前面是 DROPFILES 头，后面是以 \0 分隔、\0\0 结尾的宽字符串路径列表。
  const auto dropfiles_size =
      sizeof(Vendor::ShellApi::DROPFILES) + ((total_chars + 1) * sizeof(wchar_t));
  HGLOBAL dropfiles_handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dropfiles_size);
  if (dropfiles_handle == nullptr) {
    return std::unexpected("Failed to allocate clipboard file list, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  auto* dropfiles = static_cast<Vendor::ShellApi::DROPFILES*>(GlobalLock(dropfiles_handle));
  if (dropfiles == nullptr) {
    free_global(dropfiles_handle);
    return std::unexpected("Failed to lock clipboard file list, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  dropfiles->pFiles = sizeof(Vendor::ShellApi::DROPFILES);
  dropfiles->fWide = TRUE;

  // 把所有文件路径顺序写进 DROPFILES 后面的缓冲区。
  auto* cursor = reinterpret_cast<wchar_t*>(reinterpret_cast<std::byte*>(dropfiles) +
                                            sizeof(Vendor::ShellApi::DROPFILES));
  for (const auto& normalized_path : normalized_paths) {
    const auto byte_count = (normalized_path.size() + 1) * sizeof(wchar_t);
    std::memcpy(cursor, normalized_path.c_str(), byte_count);
    cursor += normalized_path.size() + 1;
  }
  *cursor = L'\0';

  // GlobalUnlock 返回 0 不一定代表失败。
  // 所以先清空 last error，再根据是否仍然为 NO_ERROR 来判断。
  SetLastError(NO_ERROR);
  if (!GlobalUnlock(dropfiles_handle) && Vendor::Windows::GetLastError() != NO_ERROR) {
    free_global(dropfiles_handle);
    return std::unexpected("Failed to unlock clipboard file list, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  // Preferred DropEffect 是 Windows Shell 常用的附加格式，
  // 用来告诉接收方这次剪贴板语义是“复制”还是“剪切/移动”。
  const auto preferred_drop_effect_format = RegisterClipboardFormatW(L"Preferred DropEffect");
  if (preferred_drop_effect_format == 0) {
    free_global(dropfiles_handle);
    return std::unexpected("Failed to register clipboard drop effect format, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  HGLOBAL drop_effect_handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD));
  if (drop_effect_handle == nullptr) {
    free_global(dropfiles_handle);
    return std::unexpected("Failed to allocate clipboard drop effect, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  auto* drop_effect = static_cast<DWORD*>(GlobalLock(drop_effect_handle));
  if (drop_effect == nullptr) {
    free_global(dropfiles_handle);
    free_global(drop_effect_handle);
    return std::unexpected("Failed to lock clipboard drop effect, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  *drop_effect = kClipboardDropEffectCopy;

  // 同上：GlobalUnlock 返回 0 时，需要结合 last error 判断是否真失败。
  SetLastError(NO_ERROR);
  if (!GlobalUnlock(drop_effect_handle) && Vendor::Windows::GetLastError() != NO_ERROR) {
    free_global(dropfiles_handle);
    free_global(drop_effect_handle);
    return std::unexpected("Failed to unlock clipboard drop effect, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  if (!OpenClipboard(nullptr)) {
    free_global(dropfiles_handle);
    free_global(drop_effect_handle);
    return std::unexpected("Failed to open clipboard, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  struct ClipboardCloser {
    ~ClipboardCloser() { CloseClipboard(); }
  } clipboard_closer;

  // OpenClipboard 成功后，当前进程接管剪贴板；
  // 先清空旧内容，再写入文件列表和“复制”语义。
  if (!EmptyClipboard()) {
    free_global(dropfiles_handle);
    free_global(drop_effect_handle);
    return std::unexpected("Failed to empty clipboard, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  if (SetClipboardData(Vendor::ShellApi::kCF_HDROP, dropfiles_handle) == nullptr) {
    free_global(dropfiles_handle);
    free_global(drop_effect_handle);
    return std::unexpected("Failed to set clipboard file list, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }
  dropfiles_handle = nullptr;

  // 再补一份 Preferred DropEffect，让粘贴方知道这是“复制文件”。
  if (SetClipboardData(preferred_drop_effect_format, drop_effect_handle) == nullptr) {
    free_global(drop_effect_handle);
    return std::unexpected("Failed to set clipboard drop effect, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }
  drop_effect_handle = nullptr;

  return {};
}

auto read_clipboard_text() -> std::expected<std::optional<std::string>, std::string> {
  if (!OpenClipboard(nullptr)) {
    return std::unexpected("Failed to open clipboard, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  struct ClipboardCloser {
    ~ClipboardCloser() { CloseClipboard(); }
  } clipboard_closer;

  if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    return std::optional<std::string>{std::nullopt};
  }

  auto handle = GetClipboardData(CF_UNICODETEXT);
  if (!handle) {
    return std::unexpected("Failed to get clipboard text handle, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  auto* text_ptr = static_cast<const wchar_t*>(GlobalLock(handle));
  if (!text_ptr) {
    return std::unexpected("Failed to lock clipboard text, Win32 error: " +
                           std::to_string(Vendor::Windows::GetLastError()));
  }

  struct GlobalUnlockGuard {
    HGLOBAL handle;
    ~GlobalUnlockGuard() {
      if (handle) {
        GlobalUnlock(handle);
      }
    }
  } unlock_guard{handle};

  std::wstring wide_text(text_ptr);
  return std::optional<std::string>{Utils::String::ToUtf8(wide_text)};
}

auto move_files_to_recycle_bin(const std::vector<std::filesystem::path>& paths)
    -> std::expected<void, std::string> {
  if (paths.empty()) {
    return {};
  }

  std::wstring from_buffer;
  for (const auto& path : paths) {
    if (path.empty()) {
      continue;
    }

    from_buffer.append(path.wstring());
    from_buffer.push_back(L'\0');
  }

  if (from_buffer.empty()) {
    return {};
  }

  // SHFileOperation 要求双 \0 结尾的路径列表
  from_buffer.push_back(L'\0');

  Vendor::ShellApi::SHFILEOPSTRUCTW file_op{};
  file_op.wFunc = Vendor::ShellApi::kFO_DELETE;
  file_op.pFrom = from_buffer.c_str();
  file_op.fFlags = Vendor::ShellApi::kFOF_ALLOWUNDO | Vendor::ShellApi::kFOF_NOCONFIRMATION |
                   Vendor::ShellApi::kFOF_NOERRORUI | Vendor::ShellApi::kFOF_SILENT;

  auto result = Vendor::ShellApi::SHFileOperationW(&file_op);
  if (result != 0) {
    return std::unexpected("Failed to move files to recycle bin, shell error: " +
                           std::to_string(result));
  }

  if (file_op.fAnyOperationsAborted != FALSE) {
    return std::unexpected("Move to recycle bin was aborted");
  }

  return {};
}

// 单实例互斥锁名称
constexpr auto kMutexName = L"Global\\SpinningMomo_SingleInstance_Mutex";
// 窗口类名
constexpr auto kWindowClassName = L"SpinningMomoFloatingWindowClass";

// 全局互斥锁句柄
static HANDLE g_instance_mutex = nullptr;

// 单实例检测：尝试获取单实例锁
[[nodiscard]] auto acquire_single_instance_lock() noexcept -> bool {
  g_instance_mutex = CreateMutexW(nullptr, FALSE, kMutexName);

  if (g_instance_mutex == nullptr) {
    // 创建失败，假定已有实例
    return false;
  }

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    // 互斥锁已存在，说明已有实例在运行
    CloseHandle(g_instance_mutex);
    g_instance_mutex = nullptr;
    return false;
  }

  // 成功获取锁，当前是第一个实例
  return true;
}

auto release_single_instance_lock() noexcept -> void {
  if (g_instance_mutex != nullptr) {
    CloseHandle(g_instance_mutex);
    g_instance_mutex = nullptr;
  }
}

// 激活已运行的实例窗口
auto activate_existing_instance() noexcept -> void {
  // 查找已运行实例的窗口
  HWND hwnd = FindWindowW(kWindowClassName, nullptr);
  if (hwnd) {
    // 发送自定义消息，让已有实例自己显示窗口
    // 这样可以绕过 UIPI 限制（高权限窗口已允许接收此消息）
    PostMessageW(hwnd, WM_SPINNINGMOMO_SHOW, 0, 0);
  }
}

}  // namespace Utils::System
