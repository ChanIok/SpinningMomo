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

}  // namespace Vendor::Windows