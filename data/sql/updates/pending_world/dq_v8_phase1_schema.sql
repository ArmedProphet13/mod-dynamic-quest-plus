-- DQ+ v8 Phase 1: Schema & Reference Data
-- Adds new beat columns, two primitive types, and three reference tables.
-- ONE-SHOT: run once against acore_world.

-- ─────────────────────────────────────────────────────────────────────────────
-- 1. Extend dq_archetype_beat with Phase 1 columns
-- ─────────────────────────────────────────────────────────────────────────────

ALTER TABLE `dq_archetype_beat`
  -- System 8: mechanic passive flag (0=active/needs button, 1=passive/listens from beat start)
  ADD COLUMN `mechanic_passive`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  -- System 9: choice-aware routing (0=default: next beat on success, end arc on fail)
  ADD COLUMN `choice_success_transition` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN `choice_fail_transition`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  -- System 6: animation sequences
  ADD COLUMN `entry_animation`           VARCHAR(32)      NOT NULL DEFAULT 'approaches',
  ADD COLUMN `exit_animation`            VARCHAR(32)      NOT NULL DEFAULT 'despawn',
  -- System 6: dummy spell references (0=none; spell_id from dq_dummy_spell or known DBC)
  ADD COLUMN `entry_spell`              INT UNSIGNED     NOT NULL DEFAULT 0,
  ADD COLUMN `exit_spell`              INT UNSIGNED     NOT NULL DEFAULT 0,
  ADD COLUMN `aura_spell`              INT UNSIGNED     NOT NULL DEFAULT 0,
  -- System 8 Fight handler: HP% threshold at which hostile NPC concedes (default 15%)
  ADD COLUMN `fight_threshold`          TINYINT UNSIGNED NOT NULL DEFAULT 15,
  -- System 8 Cast handler: spell school to detect (0=any, 2=Holy, 8=Nature, 32=Shadow, 64=Arcane)
  ADD COLUMN `cast_school`             TINYINT UNSIGNED NOT NULL DEFAULT 0,
  -- System 4 NPC Builder: player_level + npc_level_offset = NPC level (signed, negative = weaker)
  ADD COLUMN `npc_level_offset`        TINYINT          NOT NULL DEFAULT 0;

-- Extend mechanic enum with the two new primitives
ALTER TABLE `dq_archetype_beat`
  MODIFY COLUMN `mechanic` ENUM('witness','courier','goto','kill','activate','fight','cast')
    NOT NULL DEFAULT 'witness';

-- ─────────────────────────────────────────────────────────────────────────────
-- 2. dq_dummy_spell — System 6 Animation spell catalogue
--    Author looks up by tags; engine calls CastSpell(spell_id) for visual effect.
--    Populate further via .lookup spell in-game; add rows as needed.
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS `dq_dummy_spell` (
  `id`          INT UNSIGNED    NOT NULL AUTO_INCREMENT,
  `tags`        VARCHAR(128)    NOT NULL DEFAULT '',
  `spell_id`    INT UNSIGNED    NOT NULL DEFAULT 0,
  `description` VARCHAR(256)    NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `idx_tags` (`tags`(64))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seed: known visual/dummy spells verified in WotLK 3.3.5a
-- Add more via: .lookup spell <keyword> in-game, then INSERT here.
INSERT INTO `dq_dummy_spell` (`tags`, `spell_id`, `description`) VALUES
  ('portal,shadow,demon,entry',        22391, 'Demon Portal — dark portal materialise effect'),
  ('portal,shadow,entry,exit',         28384, 'Portal of Shadows — fel-green shadow portal'),
  ('portal,beam,entry',                32579, 'Portal Beam — arcane beam portal channel'),
  ('fade,nether,exit',                 30521, 'Nether Beam Fade — ethereal fade-out beam'),
  ('fade,vanish,exit,stealth',         24700, 'Vanish — smoke vanish effect'),
  ('portal,ileana,succubus,entry,exit',60857, 'Succubus portal entry — used for Ileana, verified in-game');

-- ─────────────────────────────────────────────────────────────────────────────
-- 3. dq_object_catalogue — System 3 Catalogue GO layer
--    Tags → gameobject_template.entry. Same tagging pattern as dq_world_npc_catalogue.
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS `dq_object_catalogue` (
  `id`          INT UNSIGNED    NOT NULL AUTO_INCREMENT,
  `tags`        VARCHAR(128)    NOT NULL DEFAULT '',
  `go_entry`    INT UNSIGNED    NOT NULL DEFAULT 0,
  `display_id`  INT UNSIGNED    NOT NULL DEFAULT 0,
  `description` VARCHAR(256)    NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `idx_tags` (`tags`(64))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seed entries added as content is authored. Example rows left as reference.
-- INSERT INTO `dq_object_catalogue` (`tags`, `go_entry`, `description`) VALUES
--   ('shrine,nature,healing,forest',  123456, 'Nature healing shrine — spirit bear quest'),
--   ('crate,supplies,wooden,merchant',789012, 'Wooden supply crate'),
--   ('portal,demonic,stone,rune',     345678, 'Demonic summoning stone');

-- ─────────────────────────────────────────────────────────────────────────────
-- 4. dq_text_variant_pool — System 7 Dialogue fallback text
--    When the author leaves chase/ambient text empty, Dialogue pulls from here
--    by matching emotion + context_tags + text_type.
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS `dq_text_variant_pool` (
  `id`           INT UNSIGNED    NOT NULL AUTO_INCREMENT,
  `emotion`      VARCHAR(32)     NOT NULL DEFAULT '',
  `context_tags` VARCHAR(128)    NOT NULL DEFAULT '',
  `text_type`    VARCHAR(32)     NOT NULL DEFAULT 'chase',
  `text`         VARCHAR(500)    NOT NULL DEFAULT '',
  `locale`       VARCHAR(4)      NOT NULL DEFAULT 'enUS',
  PRIMARY KEY (`id`),
  KEY `idx_emotion_type` (`emotion`, `text_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seed: fallback chase and ambient lines by emotion.
-- text_type values: 'chase' (player walks away), 'ambient' (periodic during quest),
--                   'concede' (NPC loses fight), 'approach' (NPC walking toward player)
INSERT INTO `dq_text_variant_pool` (`emotion`, `context_tags`, `text_type`, `text`) VALUES
  -- desperate: chase
  ('desperate', '',        'chase',   'Please. Do not go.'),
  ('desperate', '',        'chase',   'Wait. I need your help.'),
  ('desperate', '',        'chase',   'I have nowhere else to turn.'),
  -- desperate: ambient
  ('desperate', '',        'ambient', 'Please hurry.'),
  ('desperate', '',        'ambient', 'I cannot do this alone.'),
  -- weary: chase
  ('weary',     '',        'chase',   'I will not ask again.'),
  ('weary',     '',        'chase',   'This is your last chance.'),
  -- weary: ambient
  ('weary',     '',        'ambient', 'I am watching.'),
  ('weary',     'forest',  'ambient', 'The forest remembers everything.'),
  -- menacing: chase
  ('menacing',  '',        'chase',   'You cannot outrun this.'),
  ('menacing',  '',        'chase',   'There is no door that keeps me out.'),
  -- menacing: ambient
  ('menacing',  '',        'ambient', 'Tick. Tick. Tick.'),
  -- seductive: chase
  ('seductive', '',        'chase',   'You will come back. They always do.'),
  ('seductive', '',        'chase',   'Walk away if you must. I will be here.'),
  -- seductive: ambient
  ('seductive', '',        'ambient', 'Do not overthink it.'),
  -- contemptuous: chase
  ('contemptuous','',      'chase',   'Run then. It changes nothing.'),
  ('contemptuous','',      'ambient', 'Pathetic.'),
  -- grateful: ambient
  ('grateful',  '',        'ambient', 'Thank you. Truly.'),
  -- concede lines (fight path — NPC loses)
  ('menacing',  '',        'concede', 'Hmph. You are stronger than you look.'),
  ('seductive', '',        'concede', 'Ha. Perhaps I underestimated you.'),
  ('contemptuous','',      'concede', 'This is not over.'),
  ('weary',     '',        'concede', 'Enough. You have earned this.'),
  -- eerie: ghost/undead context
  ('eerie',     'ghost',   'chase',   'I have waited this long. I can wait longer.'),
  ('eerie',     'ghost',   'ambient', 'They are still here. I can feel them.');
