module;

#include <windows.h>

#include <iostream>

module UI.TrayIcon;

import std;
import Core.State;
import Core.Constants;
import UI.TrayMenu;
import UI.AppWindow.State;
import UI.TrayIcon.State;
import Vendor.Windows;
import Vendor.ShellApi;

namespace UI::TrayIcon {

auto create(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (state.tray_icon->is_created) {
    return {};
  }

  auto& nid = state.tray_icon->nid;
  nid.cbSize = sizeof(decltype(nid));
  nid.hWnd = state.app_window->window.hwnd;
  nid.uID = Core::Constants::HOTKEY_ID;
  nid.uFlags =
      Vendor::ShellApi::NIF_ICON_t | Vendor::ShellApi::NIF_MESSAGE_t | Vendor::ShellApi::NIF_TIP_t;
  nid.uCallbackMessage = Core::Constants::WM_TRAYICON;

  // 加载图标
  nid.hIcon = static_cast<HICON>(LoadImageW(
      state.app_window->window.instance, MAKEINTRESOURCEW(Core::Constants::IDI_ICON1), IMAGE_ICON,
      GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));

  if (!nid.hIcon) {
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  }

  if (!nid.hIcon) {
    return std::unexpected("Failed to load tray icon.");
  }

  // 设置工具提示文本
  const auto app_name = Core::Constants::APP_NAME;
  const auto buffer_size = std::size(nid.szTip);
  const auto copy_len = std::min(app_name.length(), buffer_size - 1);
  app_name.copy(nid.szTip, copy_len);
  nid.szTip[copy_len] = L'\0';

  if (!Vendor::ShellApi::Shell_NotifyIconW(Vendor::ShellApi::NIM_ADD_t, &nid)) {
    return std::unexpected("Failed to add tray icon to the shell.");
  }

  state.tray_icon->is_created = true;
  return {};
}

auto destroy(Core::State::AppState& state) -> void {
  if (!state.tray_icon->is_created) {
    return;
  }

  Vendor::ShellApi::Shell_NotifyIconW(Vendor::ShellApi::NIM_DELETE_t, &state.tray_icon->nid);

  if (state.tray_icon->nid.hIcon) {
    DestroyIcon(state.tray_icon->nid.hIcon);
    state.tray_icon->nid.hIcon = nullptr;
  }

  state.tray_icon->is_created = false;
}

auto show_context_menu(Core::State::AppState& state) -> void {
  Vendor::Windows::POINT pt;
  GetCursorPos(reinterpret_cast<POINT*>(&pt));

  // 使用自定义D2D菜单替代系统菜单
  UI::TrayMenu::show_menu(state, pt);
}

}  // namespace UI::TrayIcon