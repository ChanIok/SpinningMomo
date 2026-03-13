module;

export module Core.Migration;

import std;
import Core.State;

namespace Core::Migration {

export auto get_last_version() -> std::expected<std::string, std::string>;

export auto save_current_version(const std::string& version) -> std::expected<void, std::string>;

export auto compare_versions(const std::string& v1, const std::string& v2) -> int;

export auto run_migration_if_needed(Core::State::AppState& app_state) -> bool;

}  // namespace Core::Migration
