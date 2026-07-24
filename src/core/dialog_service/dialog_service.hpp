#pragma once

#include "core/state/app_state.hpp"
#include "utils/dialog/dialog.hpp"
#include "vendor/windows.hpp"

namespace Core::DialogService {

auto start(Core::State::AppState& state) -> std::expected<void, std::string>;

auto stop(Core::State::AppState& state) -> void;

auto open_file(Core::State::AppState& state, const Utils::Dialog::FileSelectorParams& params,
               Vendor::Windows::HWND hwnd = nullptr)
    -> std::expected<Utils::Dialog::FileSelectorResult, std::string>;

auto open_folder(Core::State::AppState& state, const Utils::Dialog::FolderSelectorParams& params,
                 Vendor::Windows::HWND hwnd = nullptr)
    -> std::expected<Utils::Dialog::FolderSelectorResult, std::string>;

}  // namespace Core::DialogService
