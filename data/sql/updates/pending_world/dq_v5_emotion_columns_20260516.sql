-- DynamicQuests+ v5 schema — Emotion/Emote System columns
-- ONE-SHOT: apply once, never re-run. Use dq_import_all.sql for content reseeds.
ALTER TABLE `dq_archetype_beat`
    ADD COLUMN `emotion`          VARCHAR(32)  NOT NULL DEFAULT '',
    ADD COLUMN `emotion_end`      VARCHAR(32)  NOT NULL DEFAULT '',
    ADD COLUMN `text_on_accept`   VARCHAR(500) NOT NULL DEFAULT '',
    ADD COLUMN `text_on_complete` VARCHAR(500) NOT NULL DEFAULT '';
