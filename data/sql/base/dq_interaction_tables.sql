-- DynamicQuests+ | Interaction Template and Choice Tables

CREATE TABLE IF NOT EXISTS `dq_interaction_template` (
    `id`            INT UNSIGNED    NOT NULL AUTO_INCREMENT,
    `name`          VARCHAR(64)     NOT NULL DEFAULT '',
    `theme`         VARCHAR(32)     NOT NULL DEFAULT 'generic',
    `npc_tags_req`  VARCHAR(255)    NOT NULL DEFAULT '' COMMENT 'Comma-separated required NPC tags',
    `npc_tags_excl` VARCHAR(255)    NOT NULL DEFAULT '' COMMENT 'Comma-separated excluded NPC tags',
    `zone_type`     ENUM('any','outdoor','capital','wilderness','dungeon_entrance')
                                    NOT NULL DEFAULT 'any',
    `level_min`     TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `level_max`     TINYINT UNSIGNED NOT NULL DEFAULT 80,
    `prereq_gold`   INT UNSIGNED    NOT NULL DEFAULT 0 COMMENT 'Minimum copper required (0=any)',
    `prereq_item`   INT UNSIGNED    NOT NULL DEFAULT 0 COMMENT 'Item player must have in bags (0=none)',
    `context_tags`  VARCHAR(64)     NOT NULL DEFAULT 'any' COMMENT 'traveling,grinding,idle,exploring,any',
    `tier`          TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '1=field report 2=episode',
    `mechanic_type` VARCHAR(32)     NOT NULL DEFAULT 'gossip_exchange',
    `rarity_weight`    INT             NOT NULL DEFAULT 10 COMMENT 'Higher=more common in selection',
    `combat_hp_pct`    SMALLINT        NOT NULL DEFAULT 0    COMMENT '+% max HP when DQ_OUTCOME_COMBAT fires',
    `combat_dmg_pct`   SMALLINT        NOT NULL DEFAULT 0    COMMENT '+% attack power when DQ_OUTCOME_COMBAT fires',
    `gossip_npc_text_id` INT UNSIGNED  NOT NULL DEFAULT 0    COMMENT 'npc_text.ID shown in gossip header (0=default)',
    `combat_hp_ratio`  FLOAT           NOT NULL DEFAULT 1.0  COMMENT 'HP multiplier for combat scaling (1.0=normal)',
    `combat_dmg_ratio` FLOAT           NOT NULL DEFAULT 1.0  COMMENT 'Damage multiplier for combat scaling (1.0=normal)',
    `zone_faction`     VARCHAR(16)     NOT NULL DEFAULT 'any' COMMENT 'alliance, horde, any',
    PRIMARY KEY (`id`),
    INDEX `idx_tier` (`tier`),
    INDEX `idx_level` (`level_min`, `level_max`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `dq_interaction_choice` (
    `id`              INT UNSIGNED    NOT NULL AUTO_INCREMENT,
    `template_id`     INT UNSIGNED    NOT NULL,
    `display_order`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `choice_text`     VARCHAR(255)    NOT NULL DEFAULT '',
    `prereq_item_id`  INT UNSIGNED    NOT NULL DEFAULT 0 COMMENT '0=no item required',
    `outcome_type`    ENUM('reward','deny','combat','fetch','fail','phase_advance')
                                      NOT NULL DEFAULT 'deny',
    `outcome_data`    VARCHAR(255)    NOT NULL DEFAULT '' COMMENT 'cost:gold:0.5,pool:dark_reward',
    `emote_on_select` INT UNSIGNED    NOT NULL DEFAULT 0 COMMENT 'NPC emote ID played on select',
    PRIMARY KEY (`id`),
    INDEX `idx_template` (`template_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Reward pool: items and gold awarded by MechanicGossipExchange
CREATE TABLE IF NOT EXISTS `dq_reward_pool` (
    `id`            INT UNSIGNED    NOT NULL AUTO_INCREMENT,
    `pool_name`     VARCHAR(64)     NOT NULL,
    `level_min`     TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `level_max`     TINYINT UNSIGNED NOT NULL DEFAULT 80,
    `item_entry`    INT UNSIGNED    NOT NULL DEFAULT 0 COMMENT '0=no item',
    `gold_min_copper` INT UNSIGNED  NOT NULL DEFAULT 0,
    `gold_max_copper` INT UNSIGNED  NOT NULL DEFAULT 0,
    `xp_amount`     INT UNSIGNED    NOT NULL DEFAULT 0,
    `weight`        INT UNSIGNED    NOT NULL DEFAULT 10,
    PRIMARY KEY (`id`),
    INDEX `idx_pool` (`pool_name`, `level_min`, `level_max`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- AI-injectable text variants: greetings, chase lines, gossip header text
CREATE TABLE IF NOT EXISTS `dq_text_variant` (
    `id`           INT UNSIGNED     NOT NULL AUTO_INCREMENT,
    `template_id`  INT UNSIGNED     NOT NULL COMMENT 'FK dq_interaction_template.id',
    `variant_type` ENUM('greeting','chase','gossip_header') NOT NULL,
    `text`         VARCHAR(500)     NOT NULL DEFAULT '',
    `locale`       VARCHAR(8)       NOT NULL DEFAULT 'enUS',
    `weight`       TINYINT UNSIGNED NOT NULL DEFAULT 10,
    PRIMARY KEY (`id`),
    INDEX `idx_template_type` (`template_id`, `variant_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Per-player history: see data/sql/base/db_characters/character_dq_history.sql
-- (must be imported into acore_characters, not acore_world)
