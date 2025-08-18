module;

export module Core.Initializer;

import std;
import Core.State;
import Vendor.Windows;

namespace Core::Initializer {

export auto initialize_application(Core::State::AppState& state,
                                   Vendor::Windows::HINSTANCE instance)
    -> std::expected<void, std::string>;

}