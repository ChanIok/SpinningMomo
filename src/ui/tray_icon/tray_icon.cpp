module;

#include <windows.h>

#include <iostream>

module UI.TrayIcon;

import std;
import Core.State;
import Core.Constants;
import Vendor.Windows;
import Vendor.ShellApi;

namespace {

auto create_window_selection_submenu(const Core::State::AppState& state) -> HMENU {
  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return nullptr;

  const auto& strings = *state.data.strings;
  int id = Core::Constants::ID_WINDOW_BASE;
  for (const auto& window : state.data.windows) {
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

  const auto& strings = *state.data.strings;
  for (size_t i = 0; i < state.data.ratios.size(); ++i) {
    UINT flags = MF_BYPOSITION | MF_STRING;
    if (i == state.ui.current_ratio_index) {
      flags |= MF_CHECKED;
    }
    InsertMenuW(h_menu, -1, flags, Core::Constants::ID_RATIO_BASE + i,
                state.data.ratios[i].name.c_str());
  }

  return h_menu;
}

auto create_resolution_submenu(const Core::State::AppState& state) -> HMENU {
  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return nullptr;

  for (size_t i = 0; i < state.data.resolutions.size(); ++i) {
    const auto& preset = state.data.resolutions[i];
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
    if (i == state.ui.current_resolution_index) {
      flags |= MF_CHECKED;
    }
    InsertMenuW(h_menu, -1, flags, Core::Constants::ID_RESOLUTION_BASE + i, menu_text_buffer);
  }

  return h_menu;
}

auto create_language_submenu(const Core::State::AppState& state) -> HMENU {
  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return nullptr;

  const auto& strings = *state.data.strings;
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
  const auto& strings = *state.data.strings;

  // Letterbox Mode
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING | (state.ui.letterbox_enabled ? MF_CHECKED : 0),
              Core::Constants::ID_LETTERBOX_WINDOW, strings.LETTERBOX_WINDOW.c_str());

  // Toggle Borderless
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_TOGGLE_BORDERLESS,
              strings.TOGGLE_BORDERLESS.c_str());

  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

  // Preview Window
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING | (state.ui.preview_enabled ? MF_CHECKED : 0),
              Core::Constants::ID_PREVIEW_WINDOW, strings.PREVIEW_WINDOW.c_str());

  // Overlay Window
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING | (state.ui.overlay_enabled ? MF_CHECKED : 0),
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
  nid.hWnd = state.window.hwnd;
  nid.uID = Core::Constants::HOTKEY_ID;
  nid.uFlags =
      Vendor::ShellApi::NIF_ICON_t | Vendor::ShellApi::NIF_MESSAGE_t | Vendor::ShellApi::NIF_TIP_t;
  nid.uCallbackMessage = Core::Constants::WM_TRAYICON;

  // 加载图标
  nid.hIcon = static_cast<HICON>(
      LoadImageW(state.window.instance, MAKEINTRESOURCEW(Core::Constants::IDI_ICON1), IMAGE_ICON,
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
  POINT pt;
  GetCursorPos(&pt);

  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return;

  const auto& strings = *state.data.strings;

  // Window selection submenu
  if (HMENU h_window_menu = create_window_selection_submenu(state)) {
    InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)h_window_menu,
                strings.SELECT_WINDOW.c_str());
    InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
  }

  // Ratio submenu
  if (HMENU h_ratio_menu = create_ratio_submenu(state)) {
    InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)h_ratio_menu,
                strings.WINDOW_RATIO.c_str());
  }

  // Resolution submenu
  if (HMENU h_size_menu = create_resolution_submenu(state)) {
    InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)h_size_menu,
                strings.RESOLUTION.c_str());
  }

  // Reset option
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_RESET,
              strings.RESET_WINDOW.c_str());
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

  // Screenshot options
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_CAPTURE_WINDOW,
              strings.CAPTURE_WINDOW.c_str());
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_OPEN_SCREENSHOT,
              strings.OPEN_SCREENSHOT.c_str());
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

  // Settings items
  add_settings_items(h_menu, state);

  // Language submenu
  if (HMENU h_lang_menu = create_language_submenu(state)) {
    InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)h_lang_menu,
                strings.LANGUAGE.c_str());
  }

  // Config and Exit options
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_CONFIG,
              strings.OPEN_CONFIG.c_str());
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_USER_GUIDE,
              strings.USER_GUIDE.c_str());
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_EXIT,
              strings.EXIT.c_str());

  // 显示
  SetForegroundWindow(state.window.hwnd);
  TrackPopupMenu(h_menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0,
                 state.window.hwnd, NULL);

  DestroyMenu(h_menu);
}

auto show_quick_menu(Core::State::AppState& state, const Vendor::Windows::POINT& pt) -> void {
  // Implementation will be added later
}

}  // namespace UI::TrayIcon