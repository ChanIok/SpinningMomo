#pragma once

#include "vendor/std.hpp"

// Auto-generated SQL schema header
// DO NOT EDIT - This file is generated from
// src/migrations/002_watch_root_recovery_state.sql

namespace core::migration::schema {

struct V002 {
  static constexpr std::array<std::string_view, 1> statements = {
      R"SQL(
CREATE TABLE watch_root_recovery_state (
    root_path TEXT PRIMARY KEY,
    volume_identity TEXT NOT NULL,
    journal_id INTEGER,
    checkpoint_usn INTEGER,
    rule_fingerprint TEXT NOT NULL,
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000)
)
        )SQL"};
};

}  // namespace core::migration::schema
