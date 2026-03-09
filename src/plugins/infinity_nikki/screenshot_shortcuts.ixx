module;

export module Plugins.InfinityNikki.ScreenshotShortcuts;

import std;
import Core.State;
import Plugins.InfinityNikki.Types;

namespace Plugins::InfinityNikki::ScreenshotShortcuts {

export auto initialize(
    Core::State::AppState& app_state,
    const std::function<void(const InfinityNikkiInitializeScreenshotShortcutsProgress&)>&
        progress_callback = nullptr)
    -> std::expected<InfinityNikkiInitializeScreenshotShortcutsResult, std::string>;

export auto sync(Core::State::AppState& app_state)
    -> std::expected<InfinityNikkiInitializeScreenshotShortcutsResult, std::string>;

export auto refresh_from_settings(Core::State::AppState& app_state) -> void;

export auto shutdown() -> void;

}  // namespace Plugins::InfinityNikki::ScreenshotShortcuts
