#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "utils/dialog/dialog.hpp"

namespace core::dialog_service {

auto start(core::AppState& state) -> std::expected<void, std::string>;

auto stop(core::AppState& state) -> void;

auto open_file(core::AppState& state, const utils::dialog::FileSelectorParams& params,
               HWND hwnd = nullptr)
    -> std::expected<utils::dialog::FileSelectorResult, std::string>;

auto open_folder(core::AppState& state, const utils::dialog::FolderSelectorParams& params,
                 HWND hwnd = nullptr)
    -> std::expected<utils::dialog::FolderSelectorResult, std::string>;

}  // namespace core::dialog_service
