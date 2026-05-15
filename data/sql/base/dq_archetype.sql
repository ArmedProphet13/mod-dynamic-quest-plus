-- DynamicQuests+ | Archetype Engine tables
--
-- dq_archetype       : one row per authored story instance
-- dq_archetype_beat  : one row per beat per story
--
-- pattern values:
--   sequential  — beats play in order, each advances on quest_complete
--   counter     — a single beat repeats transition_value times, then a final beat fires
--   branching   — player choice on beat 1 determines which beat fires next
--
-- mechanic values:
--   witness   — player stands in zone, NPC performs emote sequence, auto-completes after timer
--   courier   — player receives quest item, returns it to NPC or reaches destination
--   goto      — player reaches a map position (proximity trigger)
--   kill      — a specific creature_template entry is killed
--   activate  — player uses/clicks a gameobject at a location
--
-- transition_type values:
--   quest_complete   — beat ends when mechanic objective is fulfilled
--   encounter_count  — beat ends after player has entered NPC range N times (transition_value=N)
--   choice           — beat ends when player selects a choice (branching pattern)
--   timer            — beat ends after transition_value seconds of NPC presence

CREATE TABLE IF NOT EXISTS `dq_archetype` (
  `id`          INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `name`        VARCHAR(64)      NOT NULL DEFAULT '',
  `pattern`     ENUM('sequential','counter','branching') NOT NULL DEFAULT 'sequential',
  `total_beats` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `zone_id`     INT UNSIGNED     NOT NULL DEFAULT 0,  -- 0 = any zone
  `level_min`   TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `level_max`   TINYINT UNSIGNED NOT NULL DEFAULT 80,
  `enabled`     TINYINT          NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


CREATE TABLE IF NOT EXISTS `dq_archetype_beat` (
  `id`               INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `archetype_id`     INT UNSIGNED     NOT NULL,
  `beat_number`      TINYINT UNSIGNED NOT NULL,          -- 1-based; counter pattern: 1=repeating beat, 2=final beat
  `display_id`       INT UNSIGNED     NOT NULL DEFAULT 0, -- 0 = use archetype-level default from WorldCatalogue
  `zone_id`          INT UNSIGNED     NOT NULL DEFAULT 0, -- 0 = inherit from dq_archetype.zone_id
  `mechanic`         ENUM('witness','courier','goto','kill','activate') NOT NULL DEFAULT 'witness',
  `transition_type`  ENUM('quest_complete','encounter_count','choice','timer') NOT NULL DEFAULT 'quest_complete',
  `transition_value` SMALLINT UNSIGNED NOT NULL DEFAULT 1, -- N for encounter_count; seconds for timer; ignored for quest_complete
  `text_greeting`    VARCHAR(500)     NOT NULL DEFAULT '', -- NPC opening line when it appears
  `text_chase`       VARCHAR(500)     NOT NULL DEFAULT '', -- NPC line when player walks away
  `emote_on_arrive`  SMALLINT         NOT NULL DEFAULT 0,  -- emote played immediately on spawn (0 = none)
  `emote_on_complete` SMALLINT        NOT NULL DEFAULT 0,  -- emote played when beat completes (0 = none)
  `reward_pool`      VARCHAR(64)      NOT NULL DEFAULT '', -- dq_reward_pool.pool_name; empty = no reward this beat
  PRIMARY KEY (`id`),
  KEY `idx_archetype_beat` (`archetype_id`, `beat_number`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
