-- DynamicQuests+ | Collectible Entries
-- Item 900001: Wild Herb Sample — dropped by herb clusters (creature 900020)
-- Creature 900020: Herb Cluster — passive 1-HP critter spawned during collect quests
-- Creature 900030: Ritual Summoner — hostile cultist spawned in a ring during warlock ritual

-- -----------------------------------------------------------------------
-- Item: Wild Herb Sample
-- class=7 (trade goods), subclass=9 (other), grey quality, stackable 20
-- -----------------------------------------------------------------------
DELETE FROM `item_template` WHERE `entry` = 900001;
INSERT INTO `item_template`
    (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`,
     `Quality`, `AllowableClass`, `AllowableRace`, `stackable`,
     `bonding`, `description`, `Material`)
VALUES
    (900001, 7, 9, -1, 'Wild Herb Sample', 3400,
     0, -1, -1, 20,
     0, 'A bundle of wild herbs gathered from nearby wildlife.', 8);

-- -----------------------------------------------------------------------
-- Creature: Herb Cluster
-- Passive critter, HealthModifier=0.01 (~1 HP), no aggro, drops herb sample
-- -----------------------------------------------------------------------
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900020;
DELETE FROM `creature_template`       WHERE `entry`      = 900020;

INSERT INTO `creature_template`
    (`entry`, `name`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`,
     `unit_class`, `unit_flags`, `flags_extra`,
     `type`, `lootid`,
     `HealthModifier`, `DamageModifier`,
     `ScriptName`)
VALUES
    (900020, 'Herb Cluster', 1, 1,
     35, 0,
     1, 0, 2,
     1, 900020,
     0.01, 0.0,
     '');

-- Model: displayid 3477 (small critter plant model)
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES
    (900020, 0, 3477, 0.5, 1.0);

-- -----------------------------------------------------------------------
-- Loot: Herb Cluster drops 1x Wild Herb Sample, 100% chance
-- -----------------------------------------------------------------------
DELETE FROM `creature_loot_template` WHERE `Entry` = 900020;
INSERT INTO `creature_loot_template`
    (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`,
     `GroupId`, `MinCount`, `MaxCount`, `Comment`)
VALUES
    (900020, 900001, 0, 100.0, 0, 1, 0, 1, 1, 'Wild Herb Sample');

-- -----------------------------------------------------------------------
-- Creature: Ritual Summoner
-- Hostile humanoid cultist spawned in ring during warlock ritual quest.
-- HP and damage are set at runtime to player-relative values; base stats irrelevant.
-- DisplayID 15590 = robed humanoid caster figure. Replace with preferred model.
-- -----------------------------------------------------------------------
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900030;
DELETE FROM `creature_template`       WHERE `entry`      = 900030;

INSERT INTO `creature_template`
    (`entry`, `name`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`,
     `unit_class`, `unit_flags`, `flags_extra`,
     `type`, `lootid`,
     `HealthModifier`, `DamageModifier`,
     `ScriptName`)
VALUES
    (900030, 'Ritual Summoner', 1, 1,
     14, 0,
     8, 0, 0,
     7, 0,
     1.0, 1.0,
     '');

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES
    (900030, 0, 15590, 1.0, 1.0);
