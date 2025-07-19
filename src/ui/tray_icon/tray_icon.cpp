module;

#include <windows.h>

#include <iostream>

module UI.TrayIcon;

import std;
import Common.MenuData;
import Core.State;
import Core.Constants;
import Core.I18n.Types;
import UI.TrayMenu;
import Vendor.Windows;
import Vendor.ShellApi;
import Utils.String;

namespace {

// 本地化文本访问辅助结构
struct TrayStrings {
  std::wstring select_window;
  std::wstring window_ratio;
  std::wstring resolution;
  std::wstring capture_window;
  std::wstring preview_window;
  std::wstring overlay_window;
  std::wstring letterbox_window;
  std::wstring toggle_borderless;
  std::wstring modify_hotkey;
  std::wstring chinese;
  std::wstring english;
  std::wstring exit;
};

// 从i18n系统获取tray相关的本地化文本
auto get_tray_strings(const Core::I18n::Types::TextData& texts) -> TrayStrings {
  return TrayStrings{
      .select_window = Utils::String::FromUtf8(texts.window.menu.select),
      .window_ratio = Utils::String::FromUtf8(texts.window.menu.ratio),
      .resolution = Utils::String::FromUtf8(texts.window.menu.resolution),
      .capture_window = Utils::String::FromUtf8(texts.features.screenshot.menu.capture),
      .preview_window = Utils::String::FromUtf8(texts.features.preview.menu.toggle),
      .overlay_window = Utils::String::FromUtf8(texts.features.overlay.menu.toggle),
      .letterbox_window = Utils::String::FromUtf8(texts.features.letterbox.menu.toggle),
      .toggle_borderless = Utils::String::FromUtf8(texts.window.menu.toggle_borderless),
      .modify_hotkey = Utils::String::FromUtf8(texts.settings.menu.hotkey),
      .chinese = Utils::String::FromUtf8(texts.i18n.languages.zh_cn),
      .english = Utils::String::FromUtf8(texts.i18n.languages.en_us),
      .exit = Utils::String::FromUtf8(texts.system.menu.exit)};
}

auto create_window_selection_submenu(const Core::State::AppState& state) -> HMENU {
  HMENU h_menu = CreatePopupMenu();
  if (!h_menu) return nullptr;

  const auto strings = get_tray_strings(state.i18n.texts);
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

  const auto strings = get_tray_strings(state.i18n.texts);
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

  const auto strings = get_tray_strings(state.i18n.texts);
  InsertMenuW(
      h_menu, -1,
      MF_BYPOSITION | MF_STRING |
          (state.config.language.current_language == Core::Constants::LANG_ZH_CN ? MF_CHECKED : 0),
      Core::Constants::ID_LANG_ZH_CN, strings.chinese.c_str());
  InsertMenuW(
      h_menu, -1,
      MF_BYPOSITION | MF_STRING |
          (state.config.language.current_language == Core::Constants::LANG_EN_US ? MF_CHECKED : 0),
      Core::Constants::ID_LANG_EN_US, strings.english.c_str());

  return h_menu;
}

auto add_settings_items(HMENU h_menu, const Core::State::AppState& state) -> void {
  const auto strings = get_tray_strings(state.i18n.texts);

  // Letterbox Mode
  InsertMenuW(h_menu, -1,
              MF_BYPOSITION | MF_STRING | (state.app_window.ui.letterbox_enabled ? MF_CHECKED : 0),
              Core::Constants::ID_LETTERBOX_WINDOW, strings.letterbox_window.c_str());

  // Toggle Borderless
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_TOGGLE_BORDERLESS,
              strings.toggle_borderless.c_str());

  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

  // Preview Window
  InsertMenuW(h_menu, -1,
              MF_BYPOSITION | MF_STRING | (state.app_window.ui.preview_enabled ? MF_CHECKED : 0),
              Core::Constants::ID_PREVIEW_WINDOW, strings.preview_window.c_str());

  // Overlay Window
  InsertMenuW(h_menu, -1,
              MF_BYPOSITION | MF_STRING | (state.app_window.ui.overlay_enabled ? MF_CHECKED : 0),
              Core::Constants::ID_OVERLAY_WINDOW, strings.overlay_window.c_str());

  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

  // Hotkey settings
  InsertMenuW(h_menu, -1, MF_BYPOSITION | MF_STRING, Core::Constants::ID_HOTKEY,
              strings.modify_hotkey.c_str());
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