module;

export module Core.Migration;

import std;
import Core.State;

namespace Core::Migration {

export struct MigrationScript {
  std::string target_version;
  std::string description;

  std::function<std::expected<void, std::string>(Core::State::AppState&)> migration_fn;
};

export auto get_last_version() -> std::expected<std::string, std::string>;

export auto save_current_version(const std::string& version) -> std::expected<void, std::string>;

export auto run_migration_if_needed(Core::State::AppState& app_state) -> bool;

}  // namespace Core::Migration
