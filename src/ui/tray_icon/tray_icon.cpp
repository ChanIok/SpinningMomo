module;

#include <windows.h>

#include <string>

module UI.TrayIcon;

import std;
import Core.State;
import Core.I18n.Types;
import Core.I18n.State;
import Features.Settings.Menu;
import Features.WindowControl;
import UI.ContextMenu;
import UI.ContextMenu.Types;
import UI.AppWindow.State;
import UI.TrayIcon.State;
import UI.TrayIcon.Types;
import Utils.String;
import Vendor.Windows;
import Vendor.ShellApi;

namespace {

auto build_window_submenu(Core::State::AppState& state)
    -> std::vector<UI::ContextMenu::Types::MenuItem> {
  std::vector<UI::ContextMenu::Types::MenuItem> items;
  const auto& texts = state.i18n->texts;
  auto windows = Features::WindowControl::get_visible_windows();
  for (const auto& window : windows) {
    if (!window.title.empty() && window.title != L"Program Manager" &&
        window.title.find(L"SpinningMomo") == std::wstring::npos) {
      items.emplace_back(UI::ContextMenu::Types::MenuItem::window_item(window));
    }
  }
  if (items.empty()) {
    auto disabled_item = UI::ContextMenu::Types::MenuItem(
        Utils::String::FromUtf8(texts.at("menu.window_no_available")));
    disabled_item.is_enabled = false;
    items.emplace_back(std::move(disabled_item));
  }
  return items;
}

auto build_ratio_submenu(Core::State::AppState& state)
    -> std::vector<UI::ContextMenu::Types::MenuItem> {
  std::vector<UI::ContextMenu::Types::MenuItem> items;
  const auto& ratios = Features::Settings::Menu::get_ratios(*state.settings);
  for (size_t i = 0; i < ratios.size(); ++i) {
    items.emplace_back(UI::ContextMenu::Types::MenuItem::ratio_item(
        ratios[i], i, i == state.app_window->ui.current_ratio_index));
  }
  return items;
}

auto build_resolution_submenu(Core::State::AppState& state)
    -> std::vector<UI::ContextMenu::Types::MenuItem> {
  std::vector<UI::ContextMenu::Types::MenuItem> items;
  const auto& resolutions = Features::Settings::Menu::get_resolutions(*state.settings);
  for (size_t i = 0; i < resolutions.size(); ++i) {
    items.emplace_back(UI::ContextMenu::Types::MenuItem::resolution_item(
        resolutions[i], i, i == state.app_window->ui.current_resolution_index));
  }
  return items;
}

auto build_tray_menu_items(Core::State::AppState& state)
    -> std::vector<UI::ContextMenu::Types::MenuItem> {
  std::vector<UI::ContextMenu::Types::MenuItem> items;
  const auto& texts = state.i18n->texts;

  auto window_menu =
      UI::ContextMenu::Types::MenuItem(Utils::String::FromUtf8(texts.at("menu.window_select")));
  window_menu.submenu_items = build_window_submenu(state);
  items.emplace_back(std::move(window_menu));

  items.emplace_back(UI::ContextMenu::Types::MenuItem::separator());

  auto ratio_menu =
      UI::ContextMenu::Types::MenuItem(Utils::String::FromUtf8(texts.at("menu.window_ratio")));
  ratio_menu.submenu_items = build_ratio_submenu(state);
  items.emplace_back(std::move(ratio_menu));

  auto resolution_menu =
      UI::ContextMenu::Types::MenuItem(Utils::String::FromUtf8(texts.at("menu.window_resolution")));
  resolution_menu.submenu_items = build_resolution_submenu(state);
  items.emplace_back(std::move(resolution_menu));

  items.emplace_back(UI::ContextMenu::Types::MenuItem::separator());

  items.emplace_back(UI::ContextMenu::Types::MenuItem::feature_item(
      Utils::String::FromUtf8(texts.at("menu.screenshot_capture")), "screenshot.capture"));
  items.emplace_back(UI::ContextMenu::Types::MenuItem::feature_item(
      Utils::String::FromUtf8(texts.at("menu.preview_toggle")), "preview.toggle",
      state.app_window->ui.preview_enabled));
  items.emplace_back(UI::ContextMenu::Types::MenuItem::feature_item(
      Utils::String::FromUtf8(texts.at("menu.overlay_toggle")), "overlay.toggle",
      state.app_window->ui.overlay_enabled));

  items.emplace_back(UI::ContextMenu::Types::MenuItem::separator());

  items.emplace_back(UI::ContextMenu::Types::MenuItem::system_item(
      Utils::String::FromUtf8(texts.at("menu.app_main")), "app.main"));

  items.emplace_back(UI::ContextMenu::Types::MenuItem::separator());

  items.emplace_back(UI::ContextMenu::Types::MenuItem::feature_item(
      state.app_window->window.is_visible ? Utils::String::FromUtf8(texts.at("menu.float_hide"))
                                          : Utils::String::FromUtf8(texts.at("menu.float_show")),
      "app.float"));

  items.emplace_back(UI::ContextMenu::Types::MenuItem::system_item(
      Utils::String::FromUtf8(texts.at("menu.app_exit")), "app.exit"));

  return items;
}
}  // anonymous namespace

namespace UI::TrayIcon {

auto create(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (state.tray_icon->is_created) {
    return {};
  }
  auto& nid = state.tray_icon->nid;
  nid.cbSize = sizeof(decltype(nid));
  nid.hWnd = state.app_window->window.hwnd;
  nid.uID = UI::TrayIcon::Types::HOTKEY_ID;
  nid.uFlags =
      Vendor::ShellApi::kNIF_ICON | Vendor::ShellApi::kNIF_MESSAGE | Vendor::ShellApi::kNIF_TIP;
  nid.uCallbackMessage = UI::TrayIcon::Types::WM_TRAYICON;

  nid.hIcon = static_cast<HICON>(LoadImageW(
      state.app_window->window.instance, MAKEINTRESOURCEW(UI::TrayIcon::Types::IDI_ICON1),
      IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
  if (!nid.hIcon) nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  if (!nid.hIcon) return std::unexpected("Failed to load tray icon.");

  const auto app_name = UI::TrayIcon::Types::APP_NAME;
  const auto buffer_size = std::size(nid.szTip);
  const auto copy_len = std::min(app_name.length(), buffer_size - 1);
  app_name.copy(nid.szTip, copy_len);
  nid.szTip[copy_len] = L'\0';

  if (!Vendor::ShellApi::Shell_NotifyIconW(Vendor::ShellApi::kNIM_ADD, &nid)) {
    return std::unexpected("Failed to add tray icon to the shell.");
  }
  state.tray_icon->is_created = true;
  return {};
}

auto destroy(Core::State::AppState& state) -> void {
  if (!state.tray_icon->is_created) {
    return;
  }
  Vendor::ShellApi::Shell_NotifyIconW(Vendor::ShellApi::kNIM_DELETE, &state.tray_icon->nid);
  if (state.tray_icon->nid.hIcon) {
    DestroyIcon(state.tray_icon->nid.hIcon);
    state.tray_icon->nid.hIcon = nullptr;
  }
  state.tray_icon->is_created = false;
}

auto show_context_menu(Core::State::AppState& state) -> void {
  Vendor::Windows::POINT pt;
  GetCursorPos(reinterpret_cast<POINT*>(&pt));

  // Build the menu items and show the generic context menu
  auto items = build_tray_menu_items(state);
  UI::ContextMenu::Show(state, std::move(items), pt);
}

}  // namespace UI::TrayIcon