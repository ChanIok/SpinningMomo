-- Keep externally removed assets during a recovery grace period.
ALTER TABLE assets ADD COLUMN missing_at INTEGER;

CREATE INDEX idx_assets_missing_at ON assets(missing_at);
