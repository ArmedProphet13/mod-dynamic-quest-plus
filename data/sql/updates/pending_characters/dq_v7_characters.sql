-- DQ+ v7: Drop legacy Ileana tracking columns from character history.
-- Run against acore_characters. ONE-SHOT.

ALTER TABLE `character_dq_history`
  DROP COLUMN `ileana_episode`,
  DROP COLUMN `soul_debt_until`;
