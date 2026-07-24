#pragma once

#include "vendor/windows.hpp"

namespace UI::TrayIcon::Types {

// 托盘图标相关常量
constexpr Vendor::Windows::UINT WM_TRAYICON = Vendor::Windows::kWM_USER + 1;  // WM_USER + 1
constexpr Vendor::Windows::UINT HOTKEY_ID = 1;
constexpr int IDI_ICON1 = 101;
inline const std::wstring APP_NAME = L"SpinningMomo";

}  // namespace UI::TrayIcon::Types