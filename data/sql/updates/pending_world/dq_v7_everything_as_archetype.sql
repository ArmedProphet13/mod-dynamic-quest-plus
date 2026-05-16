-- DQ+ v7: Everything-as-Archetype refactor
-- Drops legacy interaction subsystem; adds beat cost/prereq columns.
-- ONE-SHOT: run once on an existing DB.

-- Drop legacy interaction tables (content replaced by archetype beats + SQL)
DROP TABLE IF EXISTS `dq_text_variant`;
DROP TABLE IF EXISTS `dq_interaction_choice`;
DROP TABLE IF EXISTS `dq_interaction_template`;

-- Add item prerequisite columns (donor beats — player must carry matching item to accept)
ALTER TABLE `dq_archetype_beat`
  ADD COLUMN `item_prereq_class`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN `item_prereq_subclass` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN `item_consume`         TINYINT UNSIGNED NOT NULL DEFAULT 0;

-- Add cost columns (high-stakes beats: Ileana gold drain / HP drain)
ALTER TABLE `dq_archetype_beat`
  ADD COLUMN `cost_gold_percent` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN `cost_hp_percent`   TINYINT UNSIGNED NOT NULL DEFAULT 0;

-- NOTE: character_dq_history Ileana columns are in acore_characters.
-- Run dq_v7_characters.sql against acore_characters to complete this migration.
