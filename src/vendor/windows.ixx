module;

#include <windows.h>

export module Vendor.Windows;

namespace Vendor::Windows {

export auto OutputDebugStringW(const wchar_t* lpOutputString) -> void {
  ::OutputDebugStringW(lpOutputString);
}

// 获取系统度量信息的包装函数
export auto GetSystemMetrics(int nIndex) -> int { return ::GetSystemMetrics(nIndex); }

// 专门的屏幕尺寸获取函数
export auto GetScreenWidth() -> int { return ::GetSystemMetrics(SM_CXSCREEN); }

export auto GetScreenHeight() -> int { return ::GetSystemMetrics(SM_CYSCREEN); }

// 消息框相关的包装函数
export auto MessageBoxW(HWND hWnd, const wchar_t* lpText, const wchar_t* lpCaption, UINT uType) -> int {
  return ::MessageBoxW(hWnd, lpText, lpCaption, uType);
}

export auto MessageBoxA(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType) -> int {
  return ::MessageBoxA(hWnd, lpText, lpCaption, uType);
}

// 消息循环相关的包装函数 - 使用明确的函数名避免宏冲突
export auto GetWindowMessage(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) -> BOOL {
  return ::GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}

export auto TranslateWindowMessage(const MSG* lpMsg) -> BOOL {
  return ::TranslateMessage(lpMsg);
}

export auto DispatchWindowMessage(const MSG* lpMsg) -> LRESULT {
  return ::DispatchMessageW(lpMsg);
}

// 应用程序控制相关的包装函数
export auto PostQuitMessage(int nExitCode) -> void {
  ::PostQuitMessage(nExitCode);
}

// 系统信息相关的包装函数
export auto GetVersionExW(LPOSVERSIONINFOW lpVersionInformation) -> BOOL {
  return ::GetVersionExW(lpVersionInformation);
}

// Windows 常量导出
export constexpr UINT MB_ICONERROR_t = 0x00000010L;

// 热键修饰符常量导出
export constexpr UINT MOD_CONTROL_t = 0x0002;
export constexpr UINT MOD_ALT_t = 0x0001;

// Windows 类型定义导出
export using HWND = ::HWND;
export using HINSTANCE = ::HINSTANCE;
export using LPWSTR = ::LPWSTR;
export using UINT = ::UINT;
export using WPARAM = ::WPARAM;
export using LPARAM = ::LPARAM;
export using LRESULT = ::LRESULT;
export using BOOL = ::BOOL;

// 消息循环相关类型
export using MSG = ::MSG;

// 系统信息相关类型
export using OSVERSIONINFOEXW = ::OSVERSIONINFOEXW;
export using LPOSVERSIONINFOW = ::LPOSVERSIONINFOW;
export using DWORD = ::DWORD;

}  // namespace Vendor::Windows