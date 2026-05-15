-- DynamicQuests+ | Lore Index table
--
-- Populated by tools/lore_scraper/scraper.py --output-sql
-- Not hand-edited. Re-run the scraper to refresh.
--
-- llm_used = 0: record has not yet been processed by the LLM pipeline
-- llm_used = 1: LLM has generated dq_text_variant rows for this entry

CREATE TABLE IF NOT EXISTS `dq_lore_index` (
  `id`              INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `zone_id`         INT UNSIGNED NOT NULL DEFAULT 0,
  `zone_name`       VARCHAR(100) NOT NULL DEFAULT '',
  `source_type`     ENUM('quest','npc','gossip') NOT NULL,
  `source_id`       INT UNSIGNED NOT NULL DEFAULT 0,
  `character_name`  VARCHAR(100) NOT NULL DEFAULT '',
  `character_title` VARCHAR(100) NOT NULL DEFAULT '',
  `quest_title`     VARCHAR(200) NOT NULL DEFAULT '',
  `emotional_tone`  VARCHAR(20)  NOT NULL DEFAULT '',
  `content_tags`    VARCHAR(255) NOT NULL DEFAULT '',
  `raw_text`        TEXT,
  `reward_text`     TEXT,
  `chain_prev`      INT NOT NULL DEFAULT 0,
  `chain_next`      INT NOT NULL DEFAULT 0,
  `llm_used`        TINYINT NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_source` (`source_type`, `source_id`),
  KEY `idx_zone`  (`zone_id`),
  KEY `idx_tone`  (`emotional_tone`),
  KEY `idx_tags`  (`content_tags`(64))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
