-- Infinity Nikki decoded photo metadata
ALTER TABLE asset_infinity_nikki_params ADD COLUMN world_id INTEGER;
ALTER TABLE asset_infinity_nikki_params ADD COLUMN world_name TEXT;
ALTER TABLE asset_infinity_nikki_params ADD COLUMN photo_mode INTEGER;
ALTER TABLE asset_infinity_nikki_params ADD COLUMN multiplayer_json TEXT;
