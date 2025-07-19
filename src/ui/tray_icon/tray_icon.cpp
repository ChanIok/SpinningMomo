module;

#include <windows.h>

#include <iostream>

module UI.TrayIcon;

import std;
import Common.MenuData;
import Core.State;
import Core.Constants;
import UI.TrayMenu;
import Vendor.Windows;
import Vendor.ShellApi;

namespace {

auto create_window_selection_submenu(const Core::State::AppState& state) -> HMENU {
  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return nullptr;

  const auto& strings = *state.app_window.data.strings;
  int id = Core::Constants::ID_WINDOW_BASE;
  for (const auto& window : state.app_window.data.windows) {
    UINT flags = MF_BYPOSITION | MF_STRING;
    if (window.title == state.config.window.title) {
      flags |= MF_CHECKED;
    }
    InsertMenuW(h_menu, -1, flags, id++, window.title.c_str());
  }

  return h_menu;
}

auto create_ratio_submenu(const Core::State::AppState& state) -> HMENU {
  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return nullptr;

  const auto& strings = *state.app_window.data.strings;
  const auto& ratios = Common::MenuData::get_current_aspect_ratios(state);
  for (size_t i = 0; i < ratios.size(); ++i) {
    UINT flags = MF_BYPOSITION | MF_STRING;
    if (i == state.app_window.ui.current_ratio_index) {
      flags |= MF_CHECKED;
    }
    InsertMenuW(h_menu, -1, flags, static_cast<UINT>(Core::Constants::ID_RATIO_BASE + i),
                ratios[i].name.c_str());
  }

  return h_menu;
}

auto create_resolution_submenu(const Core::State::AppState& state) -> HMENU {
  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return nullptr;

  const auto& resolutions = Common::MenuData::get_current_resolutions(state);
  for (size_t i = 0; i < resolutions.size(); ++i) {
    const auto& preset = resolutions[i];
    wchar_t menu_text_buffer[256];

    std::wstring formatted_text;
    if (preset.baseWidth == 0 && preset.baseHeight == 0) {
      formatted_text = preset.name;
    } else {
      formatted_text = std::format(L"{} ({:.1f}M)", preset.name, preset.totalPixels / 1000000.0);
    }

    // Copy the formatted string to the buffer for the WinAPI call
    wcscpy_s(menu_text_buffer, std::size(menu_text_buffer), formatted_text.c_str());

    UINT flags = MF_BYPOSITION | MF_STRING;
    if (i == state.app_window.ui.current_resolution_index) {
      flags |= MF_CHECKED;
    }
    InsertMenuW(h_menu, -1, flags, static_cast<UINT>(Core::Constants::ID_RESOLUTION_BASE + i),
                menu_text_buffer);
  }

  return h_menu;
}

auto create_language_submenu(const Core::State::AppState& state) -> HMENU {
  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return nullptr;

  const auto& strings = *state.app_window.data.strings;
  InsertMenuW(
      h_menu, -1,
      MF_BYPOSITION | MF_STRING |
          (state.config.language.current_language == Core::Constants::LANG_ZH_CN ? MF_CHECKED : 0),
      Core::Constants::ID_LANG_ZH_CN, strings.CHINESE.c_str());
  InsertMenuW(
      h_menu, -1,
      MF_BYPOSITION | MF_STRING |
          (state.config.language.current_language == Core::Constants::LANG_EN_US ? MF_CHECKED : 0),
      Core::Constants::ID_LANG_EN_US, strings.ENGLISH.c_str());

  return h_menu;
}

auto add_settings_items(HMENU h_menu, const Core::State::AppState& state) -> void {
  const auto& strings = *state.app_window.data.strings;

  // Letterbox Mode
  InsertMenuW(h_menu, -1,
              MF_BYPOSITION | MF_STRING | (state.app_window.ui.letterbox_enabled ? MF_CHECKED : 0),
              Core::Constants::ID_LETTERBOX_WINDOW, strings.LETTERBOX_WINDOW.c_str());

  // Toggle Borderless
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_TOGGLE_BORDERLESS,
              strings.TOGGLE_BORDERLESS.c_str());

  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

  // Preview Window
  InsertMenuW(h_menu, -1,
              MF_BYPOSITION | MF_STRING | (state.app_window.ui.preview_enabled ? MF_CHECKED : 0),
              Core::Constants::ID_PREVIEW_WINDOW, strings.PREVIEW_WINDOW.c_str());

  // Overlay Window
  InsertMenuW(h_menu, -1,
              MF_BYPOSITION | MF_STRING | (state.app_window.ui.overlay_enabled ? MF_CHECKED : 0),
              Core::Constants::ID_OVERLAY_WINDOW, strings.OVERLAY_WINDOW.c_str());

  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

  // Hotkey settings
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_HOTKEY,
              strings.MODIFY_HOTKEY.c_str());
}

}  // anonymous namespace

namespace UI::TrayIcon {

auto create(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (state.tray_icon.is_created) {
    return {};
  }

  auto& nid = state.tray_icon.nid;
  nid.cbSize = sizeof(decltype(nid));
  nid.hWnd = state.app_window.window.hwnd;
  nid.uID = Core::Constants::HOTKEY_ID;
  nid.uFlags =
      Vendor::ShellApi::NIF_ICON_t | Vendor::ShellApi::NIF_MESSAGE_t | Vendor::ShellApi::NIF_TIP_t;
  nid.uCallbackMessage = Core::Constants::WM_TRAYICON;

  // 加载图标
  nid.hIcon = static_cast<HICON>(LoadImageW(
      state.app_window.window.instance, MAKEINTRESOURCEW(Core::Constants::IDI_ICON1), IMAGE_ICON,
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

  state.tray_icon.is_created = true;
  return {};
}

auto destroy(Core::State::AppState& state) -> void {
  if (!state.tray_icon.is_created) {
    return;
  }

  Vendor::ShellApi::Shell_NotifyIconW(Vendor::ShellApi::NIM_DELETE_t, &state.tray_icon.nid);

  if (state.tray_icon.nid.hIcon) {
    DestroyIcon(state.tray_icon.nid.hIcon);
    state.tray_icon.nid.hIcon = nullptr;
  }

  state.tray_icon.is_created = false;
}

auto show_context_menu(Core::State::AppState& state) -> void {
  Vendor::Windows::POINT pt;
  GetCursorPos(reinterpret_cast<POINT*>(&pt));

  // 使用自定义D2D菜单替代系统菜单
  UI::TrayMenu::show_menu(state, pt);
}

auto show_quick_menu(Core::State::AppState& state, const Vendor::Windows::POINT& pt) -> void {
  // Implementation will be added later
}

}  // namespace UI::TrayIcon