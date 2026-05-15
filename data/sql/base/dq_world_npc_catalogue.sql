-- DynamicQuests+ | World NPC Catalogue
-- Populated by dq_bootstrap.sql. Do not edit manually.
-- Re-run bootstrap to regenerate.

CREATE TABLE IF NOT EXISTS `dq_world_npc_catalogue` (
    `entry`         INT UNSIGNED    NOT NULL,
    `zone_id`       INT UNSIGNED    NOT NULL DEFAULT 0,
    `creature_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `faction`       SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `tags`          VARCHAR(255)    NOT NULL DEFAULT '',
    `threat_level`  TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '0=safe 1=low 2=medium 3=high',
    `excluded`      TINYINT(1)      NOT NULL DEFAULT 0 COMMENT '1=boss/vehicle/trigger — never use',
    PRIMARY KEY (`entry`),
    INDEX `idx_zone` (`zone_id`),
    INDEX `idx_excluded` (`excluded`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Blacklist: famous lore characters that should never be used as couriers
CREATE TABLE IF NOT EXISTS `dq_npc_blacklist` (
    `entry`     INT UNSIGNED    NOT NULL,
    `reason`    VARCHAR(128)    NOT NULL DEFAULT '',
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `dq_npc_blacklist` VALUES
(36597, 'Lich King'),
(16980, 'Illidan'),
(17257, 'Kael''thas Sunstrider'),
(21687, 'Kil''jaeden'),
(11583, 'Lord Nefarian'),
(14888, 'Onyxia'),
(24664, 'Sartharion'),
(33288, 'Algalon');
