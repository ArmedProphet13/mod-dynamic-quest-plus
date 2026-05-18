-- DQ+ v9 Migration: Unified Emote System
-- ONE-SHOT. Removes legacy per-beat emote override columns from dq_archetype_beat.
-- Run AFTER dq_emotion.sql base schema has been applied.
-- Run BEFORE dq_import_all.sql (which no longer includes these columns).

ALTER TABLE `dq_archetype_beat`
    DROP COLUMN `emote_on_arrive`,
    DROP COLUMN `emote_on_complete`;
