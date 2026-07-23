// Auto-generated SQL schema module
// DO NOT EDIT - This file is generated from src/migrations/005_asset_missing_lifecycle.sql

module;

export module Core.Migration.Schema.V005;

import std;

namespace Core::Migration::Schema {

export struct V005 {
  static constexpr std::array<std::string_view, 2> statements = {
      R"SQL(
ALTER TABLE assets ADD COLUMN missing_at INTEGER
        )SQL",
      R"SQL(
CREATE INDEX idx_assets_missing_at ON assets(missing_at)
        )SQL"};
};

}  // namespace Core::Migration::Schema
