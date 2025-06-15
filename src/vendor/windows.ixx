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

// Windows 常量导出
export constexpr UINT MB_ICONERROR_t = 0x00000010L;

// Windows 类型定义导出
export using HWND = ::HWND;
export using HINSTANCE = ::HINSTANCE;
export using LPWSTR = ::LPWSTR;

}  // namespace Vendor::Windows