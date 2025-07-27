module;

export module UI.TrayIcon.Types;

import std;
import Vendor.Windows;

namespace UI::TrayIcon::Types {

// 托盘图标相关常量
export constexpr Vendor::Windows::UINT WM_TRAYICON = Vendor::Windows::WM_USER_t + 1;  // WM_USER + 1
export constexpr Vendor::Windows::UINT HOTKEY_ID = 1;
export constexpr int IDI_ICON1 = 101;
export const std::wstring APP_NAME = L"SpinningMomo";

}  // namespace UI::TrayIcon::Types