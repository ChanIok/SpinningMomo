-- ============================================================================
-- Watch Root Recovery State Table
-- ============================================================================
CREATE TABLE watch_root_recovery_state (
    root_path TEXT PRIMARY KEY,
    volume_identity TEXT NOT NULL,
    journal_id INTEGER,
    checkpoint_usn INTEGER,
    rule_fingerprint TEXT NOT NULL,
    updated_at INTEGER DEFAULT (unixepoch('subsec') * 1000)
);
