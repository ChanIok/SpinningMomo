-- Initialize database schema
-- This migration creates the initial database schema for SpinningMomo
-- Tables: assets, folders, tags, asset_tags, ignore_rules
-- ============================================================================
-- Assets Table
-- ============================================================================
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
    rating INTEGER NOT NULL DEFAULT 0 CHECK (
        rating BETWEEN 0
        AND 5
    ),
    review_flag TEXT NOT NULL DEFAULT 'none' CHECK (
        review_flag IN ('none', 'picked', 'rejected')
    ),
    folder_id INTEGER REFERENCES folders(id) ON DELETE
    SET
        NULL,
        file_created_at INTEGER,
        file_modified_at INTEGER,
        created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
        updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000)
);

-- ============================================================================
-- Assets Indexes
-- ============================================================================
CREATE INDEX idx_assets_path ON assets(path);

CREATE INDEX idx_assets_type ON assets(type);

CREATE INDEX idx_assets_extension ON assets(extension);

CREATE INDEX idx_assets_created_at ON assets(created_at);

CREATE INDEX idx_assets_hash ON assets(hash);

CREATE INDEX idx_assets_rating ON assets(rating);

CREATE INDEX idx_assets_review_flag ON assets(review_flag);

CREATE INDEX idx_assets_folder_id ON assets(folder_id);

CREATE INDEX idx_assets_file_created_at ON assets(file_created_at);

CREATE INDEX idx_assets_file_modified_at ON assets(file_modified_at);

CREATE INDEX idx_assets_folder_time ON assets(folder_id, file_created_at);

-- ============================================================================
-- Assets Triggers
-- ============================================================================
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

END;

-- ============================================================================
-- Folders Table
-- ============================================================================
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
);

-- ============================================================================
-- Folders Indexes
-- ============================================================================
CREATE INDEX idx_folders_parent_sort ON folders(parent_id, sort_order);

CREATE INDEX idx_folders_path ON folders(path);

-- ============================================================================
-- Folders Triggers
-- ============================================================================
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

END;

-- ============================================================================
-- Tags Table
-- ============================================================================
CREATE TABLE tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    parent_id INTEGER,
    sort_order INTEGER DEFAULT 0,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    FOREIGN KEY (parent_id) REFERENCES tags(id) ON DELETE CASCADE
);

-- ============================================================================
-- Asset Tags Junction Table
-- ============================================================================
CREATE TABLE asset_tags (
    asset_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    PRIMARY KEY (asset_id, tag_id),
    FOREIGN KEY (asset_id) REFERENCES assets(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

-- ============================================================================
-- Tags Indexes
-- ============================================================================
CREATE INDEX idx_tags_parent_sort ON tags(parent_id, sort_order);

-- ============================================================================
-- Tags Triggers
-- ============================================================================
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

END;

-- ============================================================================
-- Asset Tags Indexes
-- ============================================================================
CREATE INDEX idx_asset_tags_tag ON asset_tags(tag_id);

-- ============================================================================
-- Asset Colors Table
-- ============================================================================
CREATE TABLE asset_colors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    asset_id INTEGER NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
    r INTEGER NOT NULL CHECK (
        r BETWEEN 0
        AND 255
    ),
    g INTEGER NOT NULL CHECK (
        g BETWEEN 0
        AND 255
    ),
    b INTEGER NOT NULL CHECK (
        b BETWEEN 0
        AND 255
    ),
    lab_l REAL NOT NULL,
    lab_a REAL NOT NULL,
    lab_b REAL NOT NULL,
    weight REAL NOT NULL CHECK (
        weight > 0
        AND weight <= 1
    ),
    l_bin INTEGER NOT NULL,
    a_bin INTEGER NOT NULL,
    b_bin INTEGER NOT NULL
);

-- ============================================================================
-- Asset Colors Indexes
-- ============================================================================
CREATE INDEX idx_asset_colors_asset_id ON asset_colors(asset_id);

CREATE INDEX idx_asset_colors_asset_weight ON asset_colors(asset_id, weight DESC);

CREATE INDEX idx_asset_colors_lab_bin ON asset_colors(l_bin, a_bin, b_bin);

-- ============================================================================
-- Infinity Nikki Photo Params Table
-- ============================================================================
CREATE TABLE asset_infinity_nikki_params (
    asset_id INTEGER PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,
    uid TEXT NOT NULL,
    camera_params TEXT,
    time_day INTEGER,
    time_hour INTEGER,
    time_min INTEGER,
    time_sec REAL,
    camera_focal_length REAL,
    aperture_section INTEGER,
    filter_id TEXT,
    filter_strength REAL,
    vignette_intensity REAL,
    light_id TEXT,
    light_strength REAL,
    nikki_loc_x REAL,
    nikki_loc_y REAL,
    nikki_loc_z REAL,
    nikki_hidden INTEGER,
    pose_id INTEGER,
    dye_code TEXT,
    nikki_diy_json TEXT
);

-- ============================================================================
-- Infinity Nikki Photo Params Indexes
-- ============================================================================
CREATE INDEX idx_infinity_nikki_params_uid ON asset_infinity_nikki_params(uid);

CREATE INDEX idx_infinity_nikki_params_pose_id ON asset_infinity_nikki_params(pose_id);

-- ============================================================================
-- Infinity Nikki Clothes Table
-- ============================================================================
CREATE TABLE asset_infinity_nikki_clothes (
    asset_id INTEGER NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
    cloth_id INTEGER NOT NULL,
    PRIMARY KEY (asset_id, cloth_id)
);

-- ============================================================================
-- Infinity Nikki Clothes Indexes
-- ============================================================================
CREATE INDEX idx_infinity_nikki_clothes_cloth_id ON asset_infinity_nikki_clothes(cloth_id);

-- ============================================================================
-- Ignore Rules Table
-- ============================================================================
CREATE TABLE ignore_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    -- 如果为 NULL，表示这是一个全局规则
    -- 如果有值，它将关联到 folders 表中的一个根目录 (即 parent_id IS NULL 的记录)
    folder_id INTEGER REFERENCES folders(id) ON DELETE CASCADE,
    -- 规则模式，可以是 glob 或正则表达式
    rule_pattern TEXT NOT NULL,
    -- 规则的类型，'glob' 类似于 .gitignore, 'regex' 则是标准的正则表达式
    pattern_type TEXT NOT NULL CHECK (pattern_type IN ('glob', 'regex')) DEFAULT 'glob',
    rule_type TEXT NOT NULL CHECK (rule_type IN ('exclude', 'include')) DEFAULT 'exclude',
    is_enabled BOOLEAN NOT NULL DEFAULT 1,
    description TEXT,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    UNIQUE(folder_id, rule_pattern)
);

-- ============================================================================
-- Ignore Rules Indexes
-- ============================================================================
CREATE INDEX idx_ignore_rules_folder_id ON ignore_rules(folder_id);

CREATE INDEX idx_ignore_rules_enabled ON ignore_rules(is_enabled);

CREATE INDEX idx_ignore_rules_pattern_type ON ignore_rules(pattern_type);

CREATE UNIQUE INDEX idx_ignore_rules_global_pattern_unique ON ignore_rules(rule_pattern)
WHERE folder_id IS NULL;

-- ============================================================================
-- Ignore Rules Triggers
-- ============================================================================
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

END;
