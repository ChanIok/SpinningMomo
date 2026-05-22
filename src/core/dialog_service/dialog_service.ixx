module;

export module Core.DialogService;

import std;
import Core.State;
import Utils.Dialog;
import Vendor.Windows;

namespace Core::DialogService {

export auto start(Core::State::AppState& state) -> std::expected<void, std::string>;

export auto stop(Core::State::AppState& state) -> void;

export auto open_file(Core::State::AppState& state, const Utils::Dialog::FileSelectorParams& params,
                      Vendor::Windows::HWND hwnd = nullptr)
    -> std::expected<Utils::Dialog::FileSelectorResult, std::string>;

export auto open_folder(Core::State::AppState& state,
                        const Utils::Dialog::FolderSelectorParams& params,
                        Vendor::Windows::HWND hwnd = nullptr)
    -> std::expected<Utils::Dialog::FolderSelectorResult, std::string>;

}  // namespace Core::DialogService
