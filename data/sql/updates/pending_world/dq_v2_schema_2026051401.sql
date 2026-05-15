-- DynamicQuests+ v2 schema additions
-- ONE-SHOT: already applied. Do NOT re-run. Use dq_import_all.sql for content reseeds.
ALTER TABLE `dq_interaction_template`
    ADD COLUMN `combat_hp_pct`      SMALLINT     NOT NULL DEFAULT 0,
    ADD COLUMN `combat_dmg_pct`     SMALLINT     NOT NULL DEFAULT 0,
    ADD COLUMN `gossip_npc_text_id` INT UNSIGNED NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS `dq_text_variant` (
    `id`           INT UNSIGNED     NOT NULL AUTO_INCREMENT,
    `template_id`  INT UNSIGNED     NOT NULL,
    `variant_type` ENUM('greeting','chase','gossip_header') NOT NULL,
    `text`         VARCHAR(500)     NOT NULL DEFAULT '',
    `locale`       VARCHAR(8)       NOT NULL DEFAULT 'enUS',
    `weight`       TINYINT UNSIGNED NOT NULL DEFAULT 10,
    PRIMARY KEY (`id`),
    INDEX `idx_template_type` (`template_id`, `variant_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
