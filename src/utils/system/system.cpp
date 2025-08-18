module;

#include <windows.h>

#include <expected>
#include <string>

module Utils.System;

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

}  // namespace Utils::System