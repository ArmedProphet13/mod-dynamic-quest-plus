-- DynamicQuests+ v4 schema additions
-- ONE-SHOT: apply once, never re-run. Use dq_import_all.sql for content reseeds.
ALTER TABLE `dq_interaction_template`
    ADD COLUMN `zone_faction` ENUM('any','alliance','horde','neutral') NOT NULL DEFAULT 'any';
