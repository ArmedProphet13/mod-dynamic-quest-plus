-- DQ+ Archetype: Hungry Child (id=3)
-- Single-beat sequential encounter. Child approaches player, asks for food.
-- Requires: player carries food (ITEM_CLASS=15/ITEM_SUBCLASS_FOOD=5).
-- On accept: food item is consumed; child plays eat emote and thanks player.

DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 3;
DELETE FROM `dq_archetype`      WHERE `id` = 3;

INSERT INTO `dq_archetype`
    (`id`, `name`, `pattern`, `total_beats`, `zone_id`, `level_min`, `level_max`,
     `enabled`, `appearance`, `required_tags`, `excluded_tags`)
VALUES
    (3, 'Hungry Child', 'sequential', 1, 0, 1, 80,
     1, 'child,humanoid',
     'urban,outdoor',
     'instance,wilderness,graveyard_nearby');

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
     `cost_gold_percent`, `cost_hp_percent`)
VALUES
    (3, 1, 0, 0, 'courier',
     'quest_complete', 1,
     'Please... do you have any food? Anything at all.',
     'Please. I haven''t eaten since yesterday.',
     0, 70,
     'hungry_child_reward', 'approaches',
     0, 0, 0.0,
     'desperate', 'grateful',
     'Thank you. You are very kind.',
     'The child takes the bread and runs off, smiling.',
     15, 5, 1,
     0, 0);
