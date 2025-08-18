module;

#include <rfl.hpp>

export module Features.Settings.Migration;

import std;

namespace Features::Settings::Migration {

export auto migrate_settings(const rfl::Generic::Object& settings, int source_version)
    -> std::expected<rfl::Generic::Object, std::string>;

}  // namespace Features.Settings. Migration