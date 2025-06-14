module;

#include <windows.h>

export module Vendor.Windows;

export namespace Vendor::Windows {

void OutputDebugStringW(const wchar_t* lpOutputString) { ::OutputDebugStringW(lpOutputString); }

}  // namespace Vendor::Windows