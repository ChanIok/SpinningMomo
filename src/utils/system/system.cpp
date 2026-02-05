module;

module Utils.System;

import std;
import <windows.h>;
import Vendor.ShellApi;

namespace Utils::System {

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

// 单实例互斥锁名称
constexpr auto kMutexName = L"Global\\SpinningMomo_SingleInstance_Mutex";
// 窗口类名
constexpr auto kWindowClassName = L"SpinningMomoAppWindowClass";

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
