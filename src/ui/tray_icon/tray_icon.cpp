#include "ui/tray_icon/tray_icon.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/shellapi.hpp"

#include "core/commands/registry.hpp"
#include "core/commands/types.hpp"
#include "core/i18n/state.hpp"
#include "core/i18n/types.hpp"
#include "core/state/app_state.hpp"
#include "features/settings/menu.hpp"
#include "features/window_control/window_control.hpp"
#include "ui/context_menu/context_menu.hpp"
#include "ui/context_menu/types.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/tray_icon/state.hpp"
#include "ui/tray_icon/types.hpp"
#include "utils/string/string.hpp"

namespace ui::tray_icon::detail {

auto build_window_submenu(core::AppState& state) -> std::vector<ui::context_menu::MenuItem> {
  std::vector<ui::context_menu::MenuItem> items;
  const auto& texts = state.i18n->texts;
  auto windows = features::window_control::get_visible_windows();
  for (const auto& window : windows) {
    if (!window.title.empty() && window.title != L"Program Manager" &&
        window.title.find(L"SpinningMomo") == std::wstring::npos) {
      items.emplace_back(ui::context_menu::MenuItem::window_item(window));
    }
  }
  if (items.empty()) {
    auto disabled_item =
        ui::context_menu::MenuItem(utils::string::FromUtf8(texts.at("menu.window_no_available")));
    disabled_item.is_enabled = false;
    items.emplace_back(std::move(disabled_item));
  }
  return items;
}

auto build_ratio_submenu(core::AppState& state) -> std::vector<ui::context_menu::MenuItem> {
  std::vector<ui::context_menu::MenuItem> items;
  const auto& ratios = features::settings::menu::get_ratios(state);
  for (size_t i = 0; i < ratios.size(); ++i) {
    items.emplace_back(ui::context_menu::MenuItem::ratio_item(
        ratios[i], i, i == state.floating_window->ui.current_ratio_index));
  }
  return items;
}

auto build_resolution_submenu(core::AppState& state) -> std::vector<ui::context_menu::MenuItem> {
  std::vector<ui::context_menu::MenuItem> items;
  const auto& resolutions = features::settings::menu::get_resolutions(state);
  for (size_t i = 0; i < resolutions.size(); ++i) {
    items.emplace_back(ui::context_menu::MenuItem::resolution_item(
        resolutions[i], i, i == state.floating_window->ui.current_resolution_index));
  }
  return items;
}

auto build_tray_menu_items(core::AppState& state) -> std::vector<ui::context_menu::MenuItem> {
  std::vector<ui::context_menu::MenuItem> items;
  const auto& texts = state.i18n->texts;

  auto window_menu =
      ui::context_menu::MenuItem(utils::string::FromUtf8(texts.at("menu.window_select")));
  window_menu.submenu_items = build_window_submenu(state);
  items.emplace_back(std::move(window_menu));

  items.emplace_back(ui::context_menu::MenuItem::separator());

  auto ratio_menu =
      ui::context_menu::MenuItem(utils::string::FromUtf8(texts.at("menu.window_ratio")));
  ratio_menu.submenu_items = build_ratio_submenu(state);
  items.emplace_back(std::move(ratio_menu));

  auto resolution_menu =
      ui::context_menu::MenuItem(utils::string::FromUtf8(texts.at("menu.window_resolution")));
  resolution_menu.submenu_items = build_resolution_submenu(state);
  items.emplace_back(std::move(resolution_menu));

  items.emplace_back(ui::context_menu::MenuItem::separator());

  items.emplace_back(ui::context_menu::MenuItem::system_item(
      utils::string::FromUtf8(texts.at("menu.app_main")), "app.main"));

  items.emplace_back(ui::context_menu::MenuItem::separator());

  items.emplace_back(ui::context_menu::MenuItem::feature_item(
      state.floating_window->window.is_visible
          ? utils::string::FromUtf8(texts.at("menu.float_hide"))
          : utils::string::FromUtf8(texts.at("menu.float_show")),
      "app.float"));

  items.emplace_back(ui::context_menu::MenuItem::system_item(
      utils::string::FromUtf8(texts.at("menu.app_exit")), "app.exit"));

  return items;
}
}  // namespace ui::tray_icon::detail

namespace ui::tray_icon {

auto create(core::AppState& state) -> std::expected<void, std::string> {
  if (state.tray_icon->is_created) {
    return {};
  }
  auto& nid = state.tray_icon->nid;
  nid.cbSize = sizeof(decltype(nid));
  nid.hWnd = state.floating_window->window.hwnd;
  nid.uID = ui::tray_icon::HOTKEY_ID;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = ui::tray_icon::WM_TRAYICON;

  nid.hIcon = static_cast<HICON>(LoadImageW(
      state.floating_window->window.instance, MAKEINTRESOURCEW(ui::tray_icon::IDI_ICON1),
      IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
  if (!nid.hIcon) nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  if (!nid.hIcon) return std::unexpected("Failed to load tray icon.");

  const auto app_name = ui::tray_icon::APP_NAME;
  const auto buffer_size = std::size(nid.szTip);
  const auto copy_len = std::min(app_name.length(), buffer_size - 1);
  app_name.copy(nid.szTip, copy_len);
  nid.szTip[copy_len] = L'\0';

  if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
    return std::unexpected("Failed to add tray icon to the shell.");
  }
  state.tray_icon->is_created = true;
  return {};
}

auto destroy(core::AppState& state) -> void {
  if (!state.tray_icon->is_created) {
    return;
  }
  Shell_NotifyIconW(NIM_DELETE, &state.tray_icon->nid);
  if (state.tray_icon->nid.hIcon) {
    DestroyIcon(state.tray_icon->nid.hIcon);
    state.tray_icon->nid.hIcon = nullptr;
  }
  state.tray_icon->is_created = false;
}

auto show_context_menu(core::AppState& state) -> void {
  POINT pt;
  GetCursorPos(reinterpret_cast<POINT*>(&pt));

  // Build the menu items and show the generic context menu
  auto items = detail::build_tray_menu_items(state);
  ui::context_menu::Show(state, std::move(items), pt);
}

}  // namespace ui::tray_icon
