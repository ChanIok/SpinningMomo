module;

#include <windows.h>  // 必须放在最前面

#include <shellapi.h>
#include <shlobj_core.h>

export module Vendor.ShellApi;

namespace Vendor::ShellApi {

// 导出 Shell API 相关类型
export using NOTIFYICONDATAW = ::NOTIFYICONDATAW;
export using PNOTIFYICONDATAW = ::PNOTIFYICONDATAW;

// 导出函数
export auto Shell_NotifyIconW(DWORD dwMessage, PNOTIFYICONDATAW lpData) -> BOOL {
  return ::Shell_NotifyIconW(dwMessage, lpData);
}

// 导出常量 (使用 k 前缀风格，保持原命名大小写)
// For dwMessage parameter of Shell_NotifyIconW
export constexpr auto kNIM_ADD = NIM_ADD;
export constexpr auto kNIM_DELETE = NIM_DELETE;

// For uFlags member of NOTIFYICONDATAW
export constexpr auto kNIF_ICON = NIF_ICON;
export constexpr auto kNIF_MESSAGE = NIF_MESSAGE;
export constexpr auto kNIF_TIP = NIF_TIP;

// ShellExecute 相关类型和常量
export using SHELLEXECUTEINFOW = ::SHELLEXECUTEINFOW;
export constexpr auto kSEE_MASK_NOCLOSEPROCESS = SEE_MASK_NOCLOSEPROCESS;
export constexpr auto kSEE_MASK_NOASYNC = SEE_MASK_NOASYNC;
export constexpr auto kSW_HIDE = SW_HIDE;
export constexpr auto kSW_SHOWNORMAL = SW_SHOWNORMAL;

// ShellExecute 函数
export auto ShellExecuteExW(SHELLEXECUTEINFOW* lpExecInfo) -> BOOL {
  return ::ShellExecuteExW(lpExecInfo);
}

// 常用文件夹 ID
export const auto& kFOLDERID_LocalAppData = FOLDERID_LocalAppData;
export const auto& kFOLDERID_Videos = FOLDERID_Videos;

// SHGetKnownFolderPath 函数
export auto SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken,
                                 PWSTR* ppszPath) -> HRESULT {
  return ::SHGetKnownFolderPath(rfid, dwFlags, hToken, ppszPath);
}

// CoTaskMemFree 函数
export auto CoTaskMemFree(LPVOID pv) -> void { ::CoTaskMemFree(pv); }

}  // namespace Vendor::ShellApi
