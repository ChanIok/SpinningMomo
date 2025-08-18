module;

#include <windows.h>  // 必须放在最前面
#include <shellapi.h>

export module Vendor.ShellApi;

namespace Vendor::ShellApi {

// 导出 Shell API 相关类型
export using NOTIFYICONDATAW = ::NOTIFYICONDATAW;
export using PNOTIFYICONDATAW = ::PNOTIFYICONDATAW;

// 导出函数
export auto Shell_NotifyIconW(DWORD dwMessage, PNOTIFYICONDATAW lpData) -> BOOL {
  return ::Shell_NotifyIconW(dwMessage, lpData);
}

// 导出常量 (沿用 _t 后缀风格)
// For dwMessage parameter of Shell_NotifyIconW
export constexpr auto NIM_ADD_t = NIM_ADD;
export constexpr auto NIM_DELETE_t = NIM_DELETE;

// For uFlags member of NOTIFYICONDATAW
export constexpr auto NIF_ICON_t = NIF_ICON;
export constexpr auto NIF_MESSAGE_t = NIF_MESSAGE;
export constexpr auto NIF_TIP_t = NIF_TIP;

}  // namespace Vendor::ShellApi