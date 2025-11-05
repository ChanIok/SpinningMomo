// Auto-generated migration module
// DO NOT EDIT - This file is generated from src/migrations/001_initial_schema.sql
//
// Version: 1
// Description: Initialize database schema
// Statements: 25

module;

export module Core.Migration.Migrations.V001;

import std;

export namespace Core::Migration::Migrations::V001 {

// Migration metadata
constexpr int version = 1;
constexpr std::string_view description = "Initialize database schema";

// SQL statements
constexpr std::array<std::string_view, 25> statements = {
        R"SQL(
CREATE TABLE assets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    path TEXT NOT NULL UNIQUE,
    type TEXT NOT NULL CHECK (
        type IN ('photo', 'video', 'live_photo', 'unknown')
    ),
    description TEXT,
    width INTEGER,
    height INTEGER,
    size INTEGER,
    extension TEXT,
    mime_type TEXT,
    hash TEXT,
    folder_id INTEGER REFERENCES folders(id) ON DELETE
    SET
        NULL,
        file_created_at INTEGER,
        file_modified_at INTEGER,
        created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
        updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000)
)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_path ON assets(path)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_type ON assets(type)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_extension ON assets(extension)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_created_at ON assets(created_at)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_hash ON assets(hash)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_folder_id ON assets(folder_id)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_file_created_at ON assets(file_created_at)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_file_modified_at ON assets(file_modified_at)
        )SQL",
        R"SQL(
CREATE INDEX idx_assets_folder_time ON assets(folder_id, file_created_at)
        )SQL",
        R"SQL(
CREATE TRIGGER update_assets_updated_at
AFTER
UPDATE
    ON assets FOR EACH ROW BEGIN
UPDATE
    assets
SET
    updated_at = (unixepoch('subsec') * 1000)
WHERE
    id = NEW.id;
END
        )SQL",
        R"SQL(
CREATE TABLE folders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT NOT NULL UNIQUE,
    parent_id INTEGER REFERENCES folders(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    display_name TEXT,
    cover_asset_id INTEGER,
    sort_order INTEGER DEFAULT 0,
    is_hidden BOOLEAN DEFAULT 0,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    FOREIGN KEY (cover_asset_id) REFERENCES assets(id) ON DELETE
    SET
        NULL
)
        )SQL",
        R"SQL(
CREATE INDEX idx_folders_parent_sort ON folders(parent_id, sort_order)
        )SQL",
        R"SQL(
CREATE INDEX idx_folders_path ON folders(path)
        )SQL",
        R"SQL(
CREATE TRIGGER update_folders_updated_at
AFTER
UPDATE
    ON folders FOR EACH ROW BEGIN
UPDATE
    folders
SET
    updated_at = (unixepoch('subsec') * 1000)
WHERE
    id = NEW.id;
END
        )SQL",
        R"SQL(
CREATE TABLE tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    parent_id INTEGER,
    sort_order INTEGER DEFAULT 0,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    FOREIGN KEY (parent_id) REFERENCES tags(id) ON DELETE CASCADE
)
        )SQL",
        R"SQL(
CREATE TABLE asset_tags (
    asset_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    PRIMARY KEY (asset_id, tag_id),
    FOREIGN KEY (asset_id) REFERENCES assets(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
)
        )SQL",
        R"SQL(
CREATE INDEX idx_tags_parent_sort ON tags(parent_id, sort_order)
        )SQL",
        R"SQL(
CREATE TRIGGER update_tags_updated_at
AFTER
UPDATE
    ON tags FOR EACH ROW BEGIN
UPDATE
    tags
SET
    updated_at = (unixepoch('subsec') * 1000)
WHERE
    id = NEW.id;
END
        )SQL",
        R"SQL(
CREATE INDEX idx_asset_tags_tag ON asset_tags(tag_id)
        )SQL",
        R"SQL(
CREATE TABLE ignore_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id INTEGER REFERENCES folders(id) ON DELETE CASCADE,
    rule_pattern TEXT NOT NULL,
    pattern_type TEXT NOT NULL CHECK (pattern_type IN ('glob', 'regex')) DEFAULT 'glob',
    rule_type TEXT NOT NULL CHECK (rule_type IN ('exclude', 'include')) DEFAULT 'exclude',
    is_enabled BOOLEAN NOT NULL DEFAULT 1,
    description TEXT,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    UNIQUE(folder_id, rule_pattern)
)
        )SQL",
        R"SQL(
CREATE INDEX idx_ignore_rules_folder_id ON ignore_rules(folder_id)
        )SQL",
        R"SQL(
CREATE INDEX idx_ignore_rules_enabled ON ignore_rules(is_enabled)
        )SQL",
        R"SQL(
CREATE INDEX idx_ignore_rules_pattern_type ON ignore_rules(pattern_type)
        )SQL",
        R"SQL(
CREATE TRIGGER update_ignore_rules_updated_at
AFTER
UPDATE
    ON ignore_rules FOR EACH ROW BEGIN
UPDATE
    ignore_rules
SET
    updated_at = (unixepoch('subsec') * 1000)
WHERE
    id = NEW.id;
END
        )SQL"
};

}  // namespace Core::Migration::Migrations::V001
