module;

#include <rfl.hpp>

export module Features.Settings.Migration;

import std;

namespace Features::Settings::Migration {

export using MigrationFunction =
    std::function<std::expected<rfl::Generic::Object, std::string>(rfl::Generic::Object&)>;

export auto migrate_settings(const rfl::Generic::Object& settings, int source_version,
                             int target_version = -1)
    -> std::expected<rfl::Generic::Object, std::string>;

}  // namespace Features::Settings::Migration
