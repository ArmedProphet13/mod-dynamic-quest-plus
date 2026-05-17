-- DQ+ Archetype: Debug Witness (id=5)
-- Minimal 1-beat archetype for diagnosing gossip delivery and accept path.
--
-- Tests (in order):
--   1. Courier spawns (entry=900004, visible from start)
--   2. Courier walks to player and stops (MoveFollow → arrive)
--   3. GossipHello fires → DQClientSession::Open builds menu + stamps non-zero menuId
--   4. Client displays gossip window (the bug under investigation)
--   5. Player clicks [Alright then.] → DQClientSession::Validate passes → OnInteractionAccepted
--   6. Beat 1 completes → arc done → COOLDOWN transition
--
-- Deliberately avoids: from_portal spawn, props, fight, kill, cast, reward pools.
-- Those add code paths that are irrelevant to the gossip delivery issue.
--
-- Trigger via GM command: .dq trigger archetype 5
-- Import into: acore_world

DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 5;
DELETE FROM `dq_archetype`      WHERE `id` = 5;
DELETE FROM `dq_reward_pool`    WHERE `pool_name` = 'smoketest_reward';

-- ---------------------------------------------------------------------------
-- Archetype definition
-- ---------------------------------------------------------------------------

INSERT INTO `dq_archetype`
    (`id`, `name`, `pattern`, `total_beats`, `zone_id`, `level_min`, `level_max`,
     `enabled`, `appearance`, `required_tags`, `excluded_tags`)
VALUES
    (5, 'Debug: Simple Witness', 'sequential', 1, 0, 1, 80,
     1, 'social', '', '');

-- ---------------------------------------------------------------------------
-- Beat 1: single witness beat
--   spawn_style = approaches  : courier is visible immediately, walks to player
--   emotion = curious         : well-tested emotion path
--   mechanic = witness        : simplest mechanic — one button, click to complete
--   transition_type = quest_complete : arc ends when player accepts
--   No props, no reward, no fight, no portal
-- ---------------------------------------------------------------------------

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`, `mechanic`,
     `transition_type`, `transition_value`,
     `text_greeting`, `text_chase`,
     `emote_on_arrive`, `emote_on_complete`,
     `reward_pool`, `spawn_style`,
     `prop_entry`, `prop_count`, `prop_radius`,
     `emotion`, `emotion_end`,
     `text_on_accept`, `text_on_complete`,
     `item_prereq_class`, `item_prereq_subclass`, `item_consume`,
     `cost_gold_percent`, `cost_hp_percent`,
     `mechanic_passive`, `choice_success_transition`, `choice_fail_transition`,
     `entry_animation`, `exit_animation`,
     `entry_spell`, `exit_spell`, `aura_spell`,
     `fight_threshold`, `cast_school`, `npc_level_offset`)
VALUES
    (5, 1, 0, 0, 'witness',
     'quest_complete', 1,
     'A passing traveler catches your eye. "Pardon me. I just wanted to say — safe roads to you."',
     'Wait — I only wanted a moment of your time.',
     0, 0,
     '', 'approaches',
     0, 0, 0.0,
     'curious', '',
     'And to you.',
     '',
     0, 0, 0,
     0, 0,
     0, 0, 0,
     'approaches', 'despawn',
     0, 0, 0,
     15, 0, 0);
