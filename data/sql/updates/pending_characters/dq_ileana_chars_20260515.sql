-- DynamicQuests+ Module
-- Add Ileana episode tracking and soul debt persistence to character_dq_history.
-- Import into: acore_characters

ALTER TABLE `character_dq_history`
    ADD COLUMN `ileana_episode`  TINYINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT '0=never met, 1=ep1 done, 2=ep2 done, 3=all episodes done',
    ADD COLUMN `soul_debt_until` INT UNSIGNED     NOT NULL DEFAULT 0
        COMMENT 'Unix timestamp when Ep3 soul debt debuff expires (0 = not active)';
