#pragma once

#include "vendor/std.hpp"

// Auto-generated SQL schema header
// DO NOT EDIT - This file is generated from
// src/migrations/006_infinity_nikki_photo_metadata.sql

namespace core::migration::schema {

struct V006 {
  static constexpr std::array<std::string_view, 4> statements = {
      R"SQL(
ALTER TABLE asset_infinity_nikki_params ADD COLUMN world_id INTEGER
        )SQL",
      R"SQL(
ALTER TABLE asset_infinity_nikki_params ADD COLUMN world_name TEXT
        )SQL",
      R"SQL(
ALTER TABLE asset_infinity_nikki_params ADD COLUMN photo_mode INTEGER
        )SQL",
      R"SQL(
ALTER TABLE asset_infinity_nikki_params ADD COLUMN multiplayer_json TEXT
        )SQL"};
};

}  // namespace core::migration::schema
