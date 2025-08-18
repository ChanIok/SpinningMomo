module;

export module Features.Screenshot.Folder;

import std;
import Core.State;
import Vendor.Windows;

namespace Features::Screenshot::Folder {

export auto open_folder(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto select_folder(Core::State::AppState& state) -> std::expected<std::filesystem::path, std::string>;

}  // namespace Features::Screenshot::Folder