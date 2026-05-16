-- DynamicQuests+ System 5: Position & Context System
-- ONE-SHOT schema migration. Run once on acore_world. Do NOT re-run.

ALTER TABLE `dq_archetype`
    ADD COLUMN `required_tags` VARCHAR(255) NOT NULL DEFAULT '',
    ADD COLUMN `excluded_tags` VARCHAR(255) NOT NULL DEFAULT '';

CREATE TABLE IF NOT EXISTS `dq_area_tags` (
  `area_id`  INT UNSIGNED NOT NULL,
  `tag`      VARCHAR(64)  NOT NULL,
  PRIMARY KEY (`area_id`, `tag`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `dq_tag_hierarchy` (
  `child_tag`  VARCHAR(64) NOT NULL,
  `parent_tag` VARCHAR(64) NOT NULL,
  PRIMARY KEY (`child_tag`, `parent_tag`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `dq_landmarks` (
  `id`      INT UNSIGNED      NOT NULL AUTO_INCREMENT,
  `map_id`  SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `x`       FLOAT             NOT NULL DEFAULT 0,
  `y`       FLOAT             NOT NULL DEFAULT 0,
  `z`       FLOAT             NOT NULL DEFAULT 0,
  `tag`     VARCHAR(64)       NOT NULL,
  `radius`  FLOAT             NOT NULL DEFAULT 50.0,
  `comment` VARCHAR(128)      NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `idx_landmarks_map` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
