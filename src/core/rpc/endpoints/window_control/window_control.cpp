module;

#include <asio.hpp>

module Core.RPC.Endpoints.WindowControl;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.WindowControl;
import Utils.String;

namespace Core::RPC::Endpoints::WindowControl {

struct VisibleWindowTitleItem {
  std::string title;
};

auto is_selectable_window(const Features::WindowControl::WindowInfo& window) -> bool {
  if (window.title.empty() || window.title == L"Program Manager") {
    return false;
  }

  return window.title.find(L"SpinningMomo") == std::wstring::npos;
}

auto build_visible_window_title_items() -> std::vector<VisibleWindowTitleItem> {
  std::vector<VisibleWindowTitleItem> items;
  std::unordered_set<std::string> seen_titles;

  auto windows = Features::WindowControl::get_visible_windows();
  items.reserve(windows.size());

  for (const auto& window : windows) {
    if (!is_selectable_window(window)) {
      continue;
    }

    auto title = Utils::String::ToUtf8(window.title);
    if (title.empty() || seen_titles.contains(title)) {
      continue;
    }

    seen_titles.insert(title);
    items.push_back(VisibleWindowTitleItem{.title = std::move(title)});
  }

  return items;
}

auto handle_list_visible_windows([[maybe_unused]] Core::State::AppState& app_state,
                                 [[maybe_unused]] const EmptyParams& params)
    -> asio::awaitable<RpcResult<std::vector<VisibleWindowTitleItem>>> {
  co_return build_visible_window_title_items();
}

auto register_all(Core::State::AppState& app_state) -> void {
  register_method<EmptyParams, std::vector<VisibleWindowTitleItem>>(
      app_state, app_state.rpc->registry, "windowControl.listVisibleWindows",
      handle_list_visible_windows,
      "List current visible window titles for target window selection");
}

}  // namespace Core::RPC::Endpoints::WindowControl
