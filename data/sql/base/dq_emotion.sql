-- DynamicQuests+ | dq_emotion — unified 4-slot emotion definitions
-- Each row defines one named emotion and the four emote IDs that drive the NPC.
-- Author writes `emotion = happy` on a beat; the system handles all emote timing automatically.
--
-- Slots:
--   opener_emote   — fires when NPC stops at player (after MoveFollow ends)
--   talk_emote     — first step of the talk/emotion loop (fires when gossip opens)
--   emotion_emote  — second step of the talk/emotion loop
--   close_emote    — fires when player responds (accept or decline)
--
-- Emote IDs are HandleEmoteCommand / Emote enum values (WoW 3.3.5a client).
-- Durations are hardcoded in DQEmotionEngine::GetEmoteDuration() — not stored here.

CREATE TABLE IF NOT EXISTS `dq_emotion` (
    `name`          VARCHAR(32) NOT NULL,
    `opener_emote`  SMALLINT    NOT NULL DEFAULT 0,
    `talk_emote`    SMALLINT    NOT NULL DEFAULT 0,
    `emotion_emote` SMALLINT    NOT NULL DEFAULT 0,
    `close_emote`   SMALLINT    NOT NULL DEFAULT 0,
    PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `dq_emotion`;

INSERT INTO `dq_emotion` (`name`, `opener_emote`, `talk_emote`, `emotion_emote`, `close_emote`)
VALUES
    ('happy',        3, 396,  11,   2),   -- wave, talkex, laugh, bow
    ('sad',         24,   1,  18,  66),   -- shy, talk, cry, salute
    ('angry',       15, 396,  14, 274),   -- roar, talkex, rude, no
    ('fearful',    430,   1,  20, 389),   -- cower, talk, beg, grovel
    ('grateful',     2,   1,  21,   3),   -- bow, talk, applaud, wave
    ('pleading',    16, 396,  76, 389),   -- kneel, talkex, plead, grovel
    ('victorious',   4, 396,  23,  66),   -- cheer, talkex, flex, salute
    ('neutral',    273,   1, 274,  66);   -- yes, talk, shrug(274=no), salute
