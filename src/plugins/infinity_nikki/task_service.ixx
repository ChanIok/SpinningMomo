module;

export module Plugins.InfinityNikki.TaskService;

import std;
import Core.State;
import Features.Gallery.Types;
import Plugins.InfinityNikki.Types;

namespace Plugins::InfinityNikki::TaskService {

export auto start_initial_scan_task(
    Core::State::AppState& app_state, const Features::Gallery::Types::ScanOptions& options,
    std::function<void(const Features::Gallery::Types::ScanResult&)> post_scan_callback = {})
    -> std::expected<std::string, std::string>;

export auto start_extract_photo_params_task(
    Core::State::AppState& app_state,
    const Plugins::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request)
    -> std::expected<std::string, std::string>;

export auto start_initialize_screenshot_hardlinks_task(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string>;

}  // namespace Plugins::InfinityNikki::TaskService
