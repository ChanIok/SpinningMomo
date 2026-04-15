-- ============================================================================
-- Infinity Nikki Params: nuan5 schema replacement
-- ============================================================================
DROP TABLE IF EXISTS asset_infinity_nikki_params_v2;

DROP INDEX IF EXISTS idx_infinity_nikki_params_uid;

DROP INDEX IF EXISTS idx_infinity_nikki_params_pose_id;

CREATE TABLE asset_infinity_nikki_params_v2 (
    asset_id INTEGER PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,
    uid TEXT NOT NULL,
    camera_params TEXT,
    time_hour INTEGER,
    time_min INTEGER,
    camera_focal_length REAL,
    rotation REAL,
    aperture_value REAL,
    filter_id INTEGER,
    filter_strength REAL,
    vignette_intensity REAL,
    light_id INTEGER,
    light_strength REAL,
    vertical INTEGER,
    bloom_intensity REAL,
    bloom_threshold REAL,
    brightness REAL,
    exposure REAL,
    contrast REAL,
    saturation REAL,
    vibrance REAL,
    highlights REAL,
    shadow REAL,
    nikki_loc_x REAL,
    nikki_loc_y REAL,
    nikki_loc_z REAL,
    nikki_hidden INTEGER,
    pose_id INTEGER
);

INSERT INTO asset_infinity_nikki_params_v2 (
    asset_id, uid, camera_params,
    time_hour, time_min,
    camera_focal_length, rotation, aperture_value,
    filter_id, filter_strength, vignette_intensity,
    light_id, light_strength,
    nikki_loc_x, nikki_loc_y, nikki_loc_z,
    nikki_hidden, pose_id
)
SELECT
    asset_id,
    uid,
    camera_params,
    time_hour,
    time_min,
    camera_focal_length,
    NULL AS rotation,
    NULL AS aperture_value,
    CASE
        WHEN filter_id IS NULL THEN NULL
        WHEN trim(filter_id) <> '' AND (trim(filter_id) GLOB '[0-9]*' OR trim(filter_id) GLOB '-[0-9]*')
            THEN CAST(trim(filter_id) AS INTEGER)
        ELSE NULL
    END AS filter_id,
    filter_strength,
    vignette_intensity,
    CASE
        WHEN light_id IS NULL THEN NULL
        WHEN trim(light_id) <> '' AND (trim(light_id) GLOB '[0-9]*' OR trim(light_id) GLOB '-[0-9]*')
            THEN CAST(trim(light_id) AS INTEGER)
        ELSE NULL
    END AS light_id,
    light_strength,
    nikki_loc_x,
    nikki_loc_y,
    nikki_loc_z,
    nikki_hidden,
    pose_id
FROM asset_infinity_nikki_params;

DROP TABLE asset_infinity_nikki_params;

ALTER TABLE asset_infinity_nikki_params_v2
RENAME TO asset_infinity_nikki_params;

CREATE INDEX IF NOT EXISTS idx_infinity_nikki_params_uid ON asset_infinity_nikki_params(uid);

