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
export auto MessageBoxW(HWND hWnd, const wchar_t* lpText, const wchar_t* lpCaption, UINT uType)
    -> int {
  return ::MessageBoxW(hWnd, lpText, lpCaption, uType);
}

export auto MessageBoxA(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType) -> int {
  return ::MessageBoxA(hWnd, lpText, lpCaption, uType);
}

// 消息循环相关的包装函数 - 使用明确的函数名避免宏冲突
export auto GetWindowMessage(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
    -> BOOL {
  return ::GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}

export auto PeekMessageW(MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax,
                         UINT wRemoveMsg) -> BOOL {
  return ::PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

export auto TranslateWindowMessage(const MSG* lpMsg) -> BOOL { return ::TranslateMessage(lpMsg); }

export auto DispatchWindowMessageW(const MSG* lpMsg) -> LRESULT {
  return ::DispatchMessageW(lpMsg);
}

// 应用程序控制相关的包装函数
export auto PostQuitMessage(int nExitCode) -> void { ::PostQuitMessage(nExitCode); }

// 窗口重绘相关的包装函数
export auto InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase) -> BOOL {
  return ::InvalidateRect(hWnd, lpRect, bErase);
}

// 系统信息相关的包装函数
export auto GetVersionExW(LPOSVERSIONINFOW lpVersionInformation) -> BOOL {
  return ::GetVersionExW(lpVersionInformation);
}

// 同步相关的包装函数
export auto MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds,
                                        DWORD dwWakeMask, DWORD dwFlags) -> DWORD {
  return ::MsgWaitForMultipleObjectsEx(nCount, pHandles, dwMilliseconds, dwWakeMask, dwFlags);
}

// Windows 常量导出
export constexpr UINT MB_ICONERROR_t = 0x00000010L;

// PeekMessage 常量
export constexpr UINT PM_REMOVE_t = 0x0001;

// MsgWaitForMultipleObjectsEx 常量
export constexpr DWORD QS_ALLINPUT_t = 0x04FF;
export constexpr DWORD MWMO_INPUTAVAILABLE_t = 0x0004;

// 热键修饰符常量导出
export constexpr UINT MOD_CONTROL_t = 0x0002;
export constexpr UINT MOD_ALT_t = 0x0001;

// WM_QUIT 常量
export constexpr UINT WM_QUIT_t = 0x0012;

// Windows 类型定义导出
export using HWND = ::HWND;
export using HINSTANCE = ::HINSTANCE;
export using LPWSTR = ::LPWSTR;
export using UINT = ::UINT;
export using WPARAM = ::WPARAM;
export using LPARAM = ::LPARAM;
export using LRESULT = ::LRESULT;
export using BOOL = ::BOOL;
export using POINT = ::POINT;

// 消息循环相关类型
export using MSG = ::MSG;

// 系统信息相关类型
export using OSVERSIONINFOEXW = ::OSVERSIONINFOEXW;
export using LPOSVERSIONINFOW = ::LPOSVERSIONINFOW;
export using DWORD = ::DWORD;

// 窗口相关类型
export using RECT = ::RECT;
export using SIZE = ::SIZE;

}  // namespace Vendor::Windows