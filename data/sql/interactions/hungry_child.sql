-- DynamicQuests+ | Interaction: "Hungry Child"
-- Theme: hungry_child | Mechanic: hungry_child | Tier: 1 | Level: 1-80
--
-- A small child appears, walks toward the player crying, and asks for any food.
-- The encounter is driven entirely by DQ_HungryChildAI (entry 900040).
--   - Any CONSUMABLE/FOOD item satisfies the requirement.
--   - On accept: item consumed, reward delivered, eat+laugh+run-off animation.
--   - On deny:   short cry, child despawns.
--
-- creature_template 900040: the DQ_HungryChildAI base creature.
-- Model 338 = Human Orphan (fallback; runtime overrides to race-appropriate child model).

-- ── Creature template ──────────────────────────────────────────────────────────

DELETE FROM `creature_template` WHERE `entry` = 900040;
INSERT INTO `creature_template`
    (`entry`, `name`, `subname`, `IconName`, `gossip_menu_id`,
     `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
     `speed_walk`, `speed_run`,
     `rank`, `type`, `type_flags`,
     `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
     `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`,
     `AIName`, `MovementType`, `HoverHeight`,
     `HealthModifier`, `ManaModifier`, `ArmorModifier`, `DamageModifier`,
     `ExperienceModifier`, `RegenHealth`,
     `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
    (900040, 'Hungry Child', 'DynamicQuests+', '', 0,
     1, 80, 0, 35, 1,            -- faction 35 = Friendly; npcflag 1 = Gossip
     1.0, 1.14286,               -- walk/run speed
     0, 7, 0,                    -- rank 0, type 7 = Humanoid
     2000, 2000, 1.0, 1.0,
     1, 256, 0, 0,               -- unit_flags 256 = NON_ATTACKABLE
     '', 0, 1.0,                 -- AIName empty (C++ script via ScriptName), MovementType 0
     1.0, 1.0, 1.0, 1.0,
     1.0, 1,
     0, 'DQ_HungryChildAI', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = 900040;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES (900040, 0, 338, 0.8, 1.0);  -- 338 = Human Orphan (DisplayScale 0.8: slightly smaller than adults)

-- ── Courier theme mapping ──────────────────────────────────────────────────────
-- Maps theme 'hungry_child' → entry 900040 so NPCMatchingEngine resolves
-- the correct AI carrier entry for this mechanic type.

DELETE FROM `dq_courier_template` WHERE `theme_tag` = 'hungry_child';
INSERT INTO `dq_courier_template` (`theme_tag`, `entry`) VALUES ('hungry_child', 900040);

-- ── Gossip text ────────────────────────────────────────────────────────────────

DELETE FROM `npc_text` WHERE `ID` = 9000002;
INSERT INTO `npc_text` (`ID`, `text0_0`, `em0_0`) VALUES
(9000002, 'A small child stands in your path, eyes red from crying.\n"Please... do you have any food? I haven''t eaten since yesterday."', 0);

-- ── Interaction template ───────────────────────────────────────────────────────

DELETE FROM `dq_interaction_template` WHERE `id` = 2;
INSERT INTO `dq_interaction_template`
    (`id`, `name`, `theme`, `npc_tags_req`, `npc_tags_excl`,
     `zone_type`, `level_min`, `level_max`, `prereq_gold`,
     `context_tags`, `tier`, `mechanic_type`, `rarity_weight`,
     `combat_hp_pct`, `combat_dmg_pct`, `gossip_npc_text_id`,
     `combat_hp_ratio`, `combat_dmg_ratio`, `zone_faction`)
VALUES
    (2, 'Hungry Child', 'hungry_child',
     'child,humanoid,civilian',
     'demon,undead,hostile,horde,animal,critter,beast,elemental',
     'capital', 1, 80, 0,
     'traveling,idle,exploring', 1, 'hungry_child', 12,
     0, 0, 9000002,
     0.0, 0.0, 'alliance');

-- Choices are kept for template completeness but are not used by MechanicHungryChild.
-- The AI (DQ_HungryChildAI) builds and handles its own gossip menu.
DELETE FROM `dq_interaction_choice` WHERE `template_id` = 2;
INSERT INTO `dq_interaction_choice`
    (`template_id`, `display_order`, `choice_text`, `prereq_item_id`,
     `outcome_type`, `outcome_data`, `emote_on_select`)
VALUES
    (2, 0, 'Here you go, little one.', 0, 'reward', 'pool:hungry_child_reward', 7),
    (2, 1, 'Scram!',                   0, 'deny',   '',                         18);

-- ── Reward pool ────────────────────────────────────────────────────────────────
-- Always awards the green armor cache (900100). No gold-only rows — the child's
-- gratitude is tangible. At 60+ the cache upgrades to blue (900101).

DELETE FROM `dq_reward_pool` WHERE `pool_name` = 'hungry_child_reward';
INSERT INTO `dq_reward_pool`
    (`pool_name`, `level_min`, `level_max`, `item_entry`,
     `gold_min_copper`, `gold_max_copper`, `xp_amount`, `weight`)
VALUES
    ('hungry_child_reward',  1, 29, 900100,  5000,  15000,  600, 10),
    ('hungry_child_reward', 30, 59, 900100, 10000,  30000, 1800, 10),
    ('hungry_child_reward', 60, 80, 900101, 20000,  50000,    0, 10);

-- ── Text variants ──────────────────────────────────────────────────────────────
-- Greeting variants only — the child doesn't chase after the player.

DELETE FROM `dq_text_variant` WHERE `template_id` = 2;
INSERT INTO `dq_text_variant` (`template_id`, `variant_type`, `text`, `weight`) VALUES
    (2, 'greeting', '"Please... do you have any food? Anything at all?"',          10),
    (2, 'greeting', 'A small hand reaches up and tugs at your sleeve.',            10),
    (2, 'greeting', '"You look kind. Are you kind? I''m so hungry..."',             9),
    (2, 'greeting', '"I haven''t eaten since yesterday. Please."',                  8),
    (2, 'greeting', 'The child watches you approach, too tired to speak first.',   7);
