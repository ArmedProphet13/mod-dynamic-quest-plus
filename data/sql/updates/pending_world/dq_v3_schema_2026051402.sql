-- DynamicQuests+ v3 schema additions
-- ONE-SHOT: already applied. Do NOT re-run. Use dq_import_all.sql for content reseeds.
ALTER TABLE `dq_interaction_template`
    ADD COLUMN `combat_hp_ratio`  FLOAT NOT NULL DEFAULT 0.0,
    ADD COLUMN `combat_dmg_ratio` FLOAT NOT NULL DEFAULT 0.0;
