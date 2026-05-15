-- DynamicQuests+ | The Herb Seller — HiddenPurpose archetype (4-beat Sequential)
--
-- Story arc:
--   Beat 1 (courier)  — NPC asks player to gather herbs; his back troubles him.
--   Beat 2 (witness)  — Player sees him selling the same herbs at the market.
--   Beat 3 (courier)  — NPC gives player coin, asks them to buy a carved toy horse.
--   Beat 4 (witness)  — Player sees him give the toy to a laughing child.
--
-- Each beat recontextualises the last. By beat 4 the player understands the whole arc
-- without a single word of explicit explanation.
--
-- Zone 37 = Hillsbrad Foothills.
-- level_min=1, level_max=80 so it fires across all test characters.
-- display_id=0 on all beats: the courier uses its default social NPC model.

DELETE FROM `dq_archetype`      WHERE `id` = 2;
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 2;

INSERT INTO `dq_archetype`
    (`id`, `name`,           `pattern`,    `total_beats`, `zone_id`, `level_min`, `level_max`, `enabled`)
VALUES
    (2,   'The Herb Seller', 'sequential', 4,              37,        1,           80,          1);

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`, `mechanic`, `transition_type`,  `transition_value`,
     `text_greeting`,
     `text_chase`,
     `emote_on_arrive`, `emote_on_complete`, `reward_pool`)
VALUES
-- Beat 1: courier — player gathers herbs (acknowledge = quest accepted, then complete)
(2, 1, 0, 37, 'courier', 'quest_complete', 1,
 'Traveler! I hate to ask a stranger, but my back troubles me something fierce today. There are wild herbs growing just past the old fence — would you pick a handful? I can pay.',
 'Please — it would only take a moment!',
 25, 4, 'herb_seller_small'),

-- Beat 2: witness — player sees him selling the herbs at the market stall
(2, 2, 0, 37, 'witness', 'encounter_count', 1,
 'You catch sight of him at the market stall, calling out to buyers. The herbs you gathered are already bundled and priced.',
 '',
 5, 0, ''),

-- Beat 3: courier — NPC gives coin, asks player to buy a toy horse from east gate
(2, 3, 0, 37, 'courier', 'quest_complete', 1,
 'Ah — you again. Good timing. Here, take this coin. There is a toy seller near the east gate. Buy me a small carved horse, would you? Child-sized.',
 'It is important. Please.',
 1, 4, ''),

-- Beat 4: witness — player sees him give the toy to his child
(2, 4, 0, 37, 'witness', 'encounter_count', 1,
 'The old man is crouched in the road, holding out the wooden horse. A small boy runs to him, laughing.',
 '',
 4, 0, 'herb_seller_final');
