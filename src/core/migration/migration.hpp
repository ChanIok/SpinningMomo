#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::migration {

auto get_last_version() -> std::expected<std::string, std::string>;

auto save_current_version(const std::string& version) -> std::expected<void, std::string>;

auto compare_versions(const std::string& v1, const std::string& v2) -> int;

auto run_migration_if_needed(core::AppState& app_state) -> bool;

}  // namespace core::migration
