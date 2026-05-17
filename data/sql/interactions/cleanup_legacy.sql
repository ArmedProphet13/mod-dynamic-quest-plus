-- DQ+ | Legacy Cleanup
-- Removes all pre-archetype-engine data so testing starts clean.
-- Import into: acore_world

-- Legacy dq_archetype rows (Hungry Child id=3, Ileana id=4)
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` IN (3, 4);
DELETE FROM `dq_archetype`      WHERE `id`           IN (3, 4);

-- dq_interaction_template / dq_interaction_choice / dq_text_variant may not exist
-- in all environments — skip if not present; they are unused by the archetype engine.

-- Legacy reward pools
DELETE FROM `dq_reward_pool` WHERE `pool_name` IN
    ('hungry_child_reward', 'ileana_ep1_reward', 'ileana_ep2_reward', 'ileana_ep3_reward');
