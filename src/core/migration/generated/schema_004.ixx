// Auto-generated SQL schema module
// DO NOT EDIT - This file is generated from
// src/migrations/004_infinity_nikki_user_record_key_value.sql

module;

export module Core.Migration.Schema.V004;

import std;

export namespace Core::Migration::Schema {

struct V004 {
  static constexpr std::array<std::string_view, 9> statements = {
      R"SQL(
DROP TABLE IF EXISTS asset_infinity_nikki_user_record_v2
        )SQL",
      R"SQL(
DROP TRIGGER IF EXISTS update_asset_infinity_nikki_user_record_updated_at
        )SQL",
      R"SQL(
CREATE TABLE asset_infinity_nikki_user_record_v2 (
    asset_id INTEGER NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
    record_key TEXT NOT NULL,
    record_value TEXT NOT NULL,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    PRIMARY KEY (asset_id, record_key)
)
        )SQL",
      R"SQL(
INSERT INTO asset_infinity_nikki_user_record_v2 (
    asset_id,
    record_key,
    record_value,
    created_at,
    updated_at
)
SELECT
    asset_id,
    'code_type' AS record_key,
    code_type AS record_value,
    created_at,
    updated_at
FROM asset_infinity_nikki_user_record
WHERE code_type IS NOT NULL
  AND trim(code_type) <> ''
        )SQL",
      R"SQL(
INSERT INTO asset_infinity_nikki_user_record_v2 (
    asset_id,
    record_key,
    record_value,
    created_at,
    updated_at
)
SELECT
    asset_id,
    'code_value' AS record_key,
    code_value AS record_value,
    created_at,
    updated_at
FROM asset_infinity_nikki_user_record
WHERE code_value IS NOT NULL
  AND trim(code_value) <> ''
        )SQL",
      R"SQL(
DROP TABLE asset_infinity_nikki_user_record
        )SQL",
      R"SQL(
ALTER TABLE asset_infinity_nikki_user_record_v2
RENAME TO asset_infinity_nikki_user_record
        )SQL",
      R"SQL(
CREATE INDEX idx_infinity_nikki_user_record_key_value ON asset_infinity_nikki_user_record(record_key, record_value)
        )SQL",
      R"SQL(
CREATE TRIGGER update_asset_infinity_nikki_user_record_updated_at
AFTER
UPDATE
    ON asset_infinity_nikki_user_record FOR EACH ROW BEGIN
UPDATE
    asset_infinity_nikki_user_record
SET
    updated_at = (unixepoch('subsec') * 1000)
WHERE
    asset_id = NEW.asset_id
    AND record_key = NEW.record_key;
END
        )SQL"};
};

}  // namespace Core::Migration::Schema
