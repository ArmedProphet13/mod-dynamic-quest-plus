-- DynamicQuests+ Module
-- Import this file into: acore_characters
-- Per-player interaction history, cooldown state, and escalation counter.

CREATE TABLE IF NOT EXISTS `character_dq_history` (
    `guid`               INT UNSIGNED     NOT NULL COMMENT 'Character GUID',
    `last_template_1`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_2`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_3`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_4`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_5`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_6`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_7`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_8`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_9`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_template_10`   INT UNSIGNED     NOT NULL DEFAULT 0,
    `last_courier_theme` VARCHAR(32)      NOT NULL DEFAULT '',
    `last_episode_id`    INT UNSIGNED     NOT NULL DEFAULT 0,
    `next_trigger_time`  INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when cooldown expires',
    `consecutive_tier1`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='DQ+ per-player state persisted across sessions';
