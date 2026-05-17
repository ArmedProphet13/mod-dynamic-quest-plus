-- DQ+ Archetype: Wanderer's Trial (id=5)
-- Smoke-test archetype covering all 6 mechanics and all transition types.
--
-- Beat map:
--   1  WITNESS   quest_complete  -- introduction, from_portal entry
--   2  WITNESS   timer=10        -- brief scene, auto-advance (no player input)
--   3  KILL      quest_complete  -- kill 1 creature of any type
--   4  ACTIVATE  quest_complete  -- click 2 herb bundles scattered nearby
--   5  FIGHT     quest_complete  -- fight courier to 20% HP (concede triggers)
--   6  CAST      quest_complete  -- cast any spell on courier (passive detection)
--   7  WITNESS   quest_complete  -- farewell + reward delivery
--
-- Trigger via GM command: .dq trigger archetype 5
-- Courier: entry 900004 (Traveling Stranger / social appearance)
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
    (5, 'Wanderer''s Trial', 'sequential', 7, 0, 1, 80,
     1, 'social', '', '');

-- ---------------------------------------------------------------------------
-- Beats
-- Columns: archetype_id, beat_number, display_id, zone_id, mechanic,
--          transition_type, transition_value,
--          text_greeting, text_chase,
--          emote_on_arrive, emote_on_complete,
--          reward_pool, spawn_style,
--          prop_entry, prop_count, prop_radius,
--          emotion, emotion_end,
--          text_on_accept, text_on_complete,
--          item_prereq_class, item_prereq_subclass, item_consume,
--          cost_gold_percent, cost_hp_percent,
--          mechanic_passive, choice_success_transition, choice_fail_transition,
--          entry_animation, exit_animation,
--          entry_spell, exit_spell, aura_spell,
--          fight_threshold, cast_school, npc_level_offset
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

-- Beat 1: Introduction — witness, portal entry, player must click [Begin]
-- Tests: from_portal entry animation, basic WITNESS mechanic, gossip menu
(5, 1, 0, 0, 'witness',
 'quest_complete', 1,
 'I have been watching you, traveler. I am the Wanderer, and you will be tested.',
 'Do not walk away. The trial has already begun.',
 1, 0,
 '', 'from_portal',
 0, 0, 0.0,
 'curious', '',
 'I am ready.',
 '',
 0, 0, 0,
 0, 0,
 0, 0, 0,
 'from_portal', 'despawn',
 0, 0, 0,
 15, 0, 0),

-- Beat 2: Observation — witness, 10-second timer, auto-advances without input
-- Tests: DQ_TRANS_TIMER, OnUpdate countdown, CompleteBeat from timer expiry
(5, 2, 0, 0, 'witness',
 'timer', 10,
 'Watch closely. This world is full of threats... and of things left unguarded.',
 '',
 1, 0,
 '', 'approaches',
 0, 0, 0.0,
 'serious', '',
 '',
 '',
 0, 0, 0,
 0, 0,
 0, 0, 0,
 'approaches', 'despawn',
 0, 0, 0,
 15, 0, 0),

-- Beat 3: Kill objective — kill 1 creature of any type
-- Tests: BeginKillObjective, OnKill, objectiveCount tracking
(5, 3, 0, 0, 'kill',
 'quest_complete', 1,
 'Something threatens this place. Deal with it, then return.',
 'Do not hesitate. The threat is real.',
 1, 0,
 '', 'approaches',
 0, 0, 0.0,
 'serious', '',
 'It will be done.',
 'Good. You did not hesitate.',
 0, 0, 0,
 0, 0,
 0, 0, 0,
 'approaches', 'despawn',
 0, 0, 0,
 15, 0, 0),

-- Beat 4: Activate objective — click 2 herb bundles scattered nearby
-- Tests: prop scatter (propEntry/propCount/propRadius), OnActivate x2,
--        auxGuidList tracking, GO deletion on click
(5, 4, 0, 0, 'activate',
 'quest_complete', 2,
 'I scattered supplies across this area. Retrieve them both.',
 'The supplies will not gather themselves.',
 1, 0,
 '', 'approaches',
 900100, 2, 8.0,
 'helpful', '',
 'I''ll find them.',
 'Well done. Now the true test begins.',
 0, 0, 0,
 0, 0,
 0, 0, 0,
 'approaches', 'despawn',
 0, 0, 0,
 15, 0, 0),

-- Beat 5: Fight — courier flips hostile, concedes at 20% HP
-- Tests: BeginFight, DQNPCBuilder::FlipToHostile, OnCourierTookDamage (passive),
--        OnCourierConceded, OnFightConcede, DQNPCBuilder::FlipToFriendly, CompleteBeat
(5, 5, 0, 0, 'fight',
 'quest_complete', 1,
 'Face me. Prove you can survive what this world will throw at you.',
 '',
 1, 0,
 '', 'approaches',
 0, 0, 0.0,
 'angry', 'satisfied',
 '',
 '',
 0, 0, 0,
 0, 0,
 0, 0, 0,
 'approaches', 'despawn',
 0, 0, 0,
 20, 0, 0),

-- Beat 6: Cast — passive; player must cast any targeted spell on the courier
-- Tests: mechanicPassive=1 gossip ("Understood." button), RegisterPassiveCast,
--        DQ_SpellCastHook, OnPlayerCastOnCourier, target GUID check, school=0 (any)
(5, 6, 0, 0, 'cast',
 'quest_complete', 1,
 'I am wounded from our duel. Cast any spell upon me. Any will do.',
 '',
 1, 0,
 '', 'approaches',
 0, 0, 0.0,
 'hurt', 'grateful',
 '',
 'Your power is noted. The trial nears its end.',
 0, 0, 0,
 0, 0,
 1, 0, 0,
 'approaches', 'despawn',
 0, 0, 0,
 15, 0, 0),

-- Beat 7: Farewell — witness, quest_complete, reward delivered, fade_out exit
-- Tests: reward pool delivery, textOnComplete, fade_out exit animation, arc completion
(5, 7, 0, 0, 'witness',
 'quest_complete', 1,
 'You have passed the Wanderer''s Trial. Take this — you have earned it.',
 '',
 1, 0,
 'smoketest_reward', 'approaches',
 0, 0, 0.0,
 'happy', 'content',
 'Thank you.',
 'Until we meet again. Safe travels, champion.',
 0, 0, 0,
 0, 0,
 0, 0, 0,
 'approaches', 'fade_out',
 0, 0, 0,
 15, 0, 0);

-- ---------------------------------------------------------------------------
-- Reward pool — gold + XP only (no item dependency)
-- ---------------------------------------------------------------------------

INSERT INTO `dq_reward_pool`
    (`pool_name`, `level_min`, `level_max`, `item_entry`,
     `gold_min_copper`, `gold_max_copper`, `xp_amount`, `weight`)
VALUES
    ('smoketest_reward',  1, 29, 0,  5000,  15000,  500, 10),
    ('smoketest_reward', 30, 59, 0, 10000,  30000, 1500, 10),
    ('smoketest_reward', 60, 79, 0, 20000,  50000, 3000, 10),
    ('smoketest_reward', 80, 80, 0, 50000, 100000,    0, 10);
