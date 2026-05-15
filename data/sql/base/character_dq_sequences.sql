-- DynamicQuests+ | Per-player archetype beat progress
-- Lives in acore_characters (character data, not world content)
--
-- One row per (player, archetype). Tracks which beat the player is on,
-- how many times they have triggered the current beat (for counter pattern),
-- and whether the archetype is fully complete.
--
-- current_beat  : next beat to fire (1-based). Set to 1 on first encounter.
-- beat_count    : times the current beat has been triggered. Used by counter pattern.
-- completed     : 1 when all beats done; archetype will not trigger again for this player.
-- last_seen     : unix timestamp of last encounter. Cooldown / future time-gap beats.
--
-- Migration note: existing ileana_episode in character_dq_history maps to archetype_id=1.
-- DynamicQuestMgr::ApplyLoginData() copies ileana_episode → this table on first login
-- if no row exists for (guid, 1). After migration, ileana_episode is ignored.

CREATE TABLE IF NOT EXISTS `character_dq_sequences` (
  `guid`         INT UNSIGNED     NOT NULL,
  `archetype_id` INT UNSIGNED     NOT NULL,
  `current_beat` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `beat_count`   SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `completed`    TINYINT          NOT NULL DEFAULT 0,
  `last_seen`    INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`, `archetype_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
