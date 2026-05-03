DROP TABLE IF EXISTS asset_infinity_nikki_user_record_v2;

DROP TRIGGER IF EXISTS update_asset_infinity_nikki_user_record_updated_at;

CREATE TABLE asset_infinity_nikki_user_record_v2 (
    asset_id INTEGER NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
    record_key TEXT NOT NULL,
    record_value TEXT NOT NULL,
    created_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000),
    PRIMARY KEY (asset_id, record_key)
);

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
  AND trim(code_type) <> '';

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
  AND trim(code_value) <> '';

DROP TABLE asset_infinity_nikki_user_record;

ALTER TABLE asset_infinity_nikki_user_record_v2
RENAME TO asset_infinity_nikki_user_record;

CREATE INDEX idx_infinity_nikki_user_record_key_value ON asset_infinity_nikki_user_record(record_key, record_value);

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

END;
