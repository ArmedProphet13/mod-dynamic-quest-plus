-- DynamicQuests+ | Prune: Remove deprecated interactions
-- Date: 2026-05-15
--
-- Removes 5 unfinished interactions that are not being shipped:
--   Template 1  — Deal with the Devil   (gossip_exchange)
--   Template 3  — Warlock Ritual        (ritual)
--   Template 4  — Herbalist's Request   (collect)
--   Template 5  — Patrol Warning        (kill)
--   Template 6  — Lost Caravan Supplies (fetch)
--
-- Surviving interactions: Template 2 (Hungry Child), Templates 10/11/12 (Ileana eps 1/2/3).
-- NOTE: Template 10 = Ileana Episode 1. DO NOT add it to this DELETE list.
-- Courier/destination creature_template entries (900001-900015, 900030) are left
-- in place; their C++ AI is removed so they are unreachable at runtime.

-- ── Interaction core tables ───────────────────────────────────────────────────

DELETE FROM `dq_interaction_template`
WHERE `id` IN (1, 3, 4, 5, 6);

DELETE FROM `dq_interaction_choice`
WHERE `template_id` IN (1, 3, 4, 5, 6);

DELETE FROM `dq_text_variant`
WHERE `template_id` IN (1, 3, 4, 5, 6);

-- ── Reward pools ─────────────────────────────────────────────────────────────
-- Only pools exclusive to the deleted interactions.
-- Shared cache pools (hungry_child_reward, ileana pools) are intentionally untouched.

DELETE FROM `dq_reward_pool`
WHERE `pool_name` IN (
    'bandit_paid',
    'bandit_loot',
    'bandit_bluff',
    'herbalist_reward',
    'demon_deal',
    'demon_kill',
    'caravan_reward',
    'patrol_reward',
    'ritual_reward',
    'demon_slayer'
);

-- ── npc_text gossip headers ───────────────────────────────────────────────────
-- IDs 9000001-9000006 and 9000010 were created exclusively for deleted interactions.
-- ID 9000002 (Hungry Child) is intentionally NOT deleted.

DELETE FROM `npc_text`
WHERE `ID` IN (9000001, 9000003, 9000004, 9000005, 9000006, 9000010);
