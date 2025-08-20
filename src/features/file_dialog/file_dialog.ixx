module;

export module Features.FileDialog;

import std;
import Core.State;
import Features.FileDialog.Types;

namespace Features::FileDialog {

export auto select_folder(Core::State::AppState& state,
                          const Features::FileDialog::Types::FolderSelectorParams& params)
    -> std::expected<Features::FileDialog::Types::FolderSelectorResult, std::string>;

export auto select_file(Core::State::AppState& state,
                        const Features::FileDialog::Types::FileSelectorParams& params)
    -> std::expected<Features::FileDialog::Types::FileSelectorResult, std::string>;

}  // namespace Features::FileDialog