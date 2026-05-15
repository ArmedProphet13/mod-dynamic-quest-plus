-- DynamicQuests+ Module
-- Ileana NPC entry, courier remap, and legacy demon-deal cleanup.
-- Import into: acore_world

-- ── 1. Remap entry 900001 away from 'demon' — Ileana is the sole demon courier now ──
UPDATE `dq_courier_template` SET `theme_tag` = 'demon_unused' WHERE `entry` = 900001;

-- ── 2. Ileana — Rare-Elite Succubus (entry 900020) ───────────────────────────────────
DELETE FROM `creature_template`        WHERE `entry` = 900020;
DELETE FROM `creature_template_model`  WHERE `CreatureID` = 900020;
DELETE FROM `creature_loot_template`   WHERE `Entry` = 900020;

INSERT INTO `creature_template`
    (`entry`, `name`, `subname`,
     `minlevel`, `maxlevel`, `faction`, `rank`,
     `BaseAttackTime`, `RangeAttackTime`, `unit_class`,
     `type`, `type_flags`, `npcflag`,
     `ScriptName`, `AIName`, `MovementType`,
     `DamageModifier`, `ArmorModifier`)
VALUES
    (900020, 'Ileana', 'Mysterious Demon',
     1, 80, 35, 2,
     2000, 2500, 1,
     3, 0, 1,
     'DQ_SuccubusAI', '', 0,
     1.0, 1.0);

-- Display: Illidari Succubus (displayId 22860, verified)
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES
    (900020, 0, 22860, 1.0, 1.0);

-- ── 3. Add Ileana to the demon courier theme ─────────────────────────────────────────
DELETE FROM `dq_courier_template` WHERE `entry` = 900020;
INSERT INTO `dq_courier_template` (`entry`, `theme_tag`, `description`) VALUES
    (900020, 'demon', 'Ileana — succubus encounter (episodes 1-3)');

-- ── 4. Deprecate legacy demon-deal template (ID 1) — replaced by episodes 10-12 ─────
DELETE FROM `dq_interaction_template` WHERE `id` = 1;
DELETE FROM `dq_interaction_choice`   WHERE `template_id` = 1;
DELETE FROM `dq_text_variant`         WHERE `template_id` = 1;
DELETE FROM `npc_text`                WHERE `ID` = 9000001;
DELETE FROM `dq_reward_pool`          WHERE `pool_name` IN ('demon_deal', 'demon_kill');
