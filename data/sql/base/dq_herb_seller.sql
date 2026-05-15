-- DynamicQuests+ | The Herb Seller — HiddenPurpose archetype (4-beat Sequential)
--
-- Story arc:
--   Beat 1 (activate) — Hopeful old man with a bad back asks for herbs; player gathers them.
--   Beat 2 (witness)  — Player sees him selling the same herbs at the market.
--   Beat 3 (courier)  — Grateful, he gives player a coin to buy a carved toy horse.
--   Beat 4 (witness)  — Player sees him give the toy to his laughing child.
--
-- Each beat recontextualises the last. By beat 4 the player understands the whole arc.
-- Zone 37 = Hillsbrad Foothills. level_min=1, level_max=80.
-- appearance 'humanoid,male' — engine picks a zone-appropriate male model.
-- emotion data drives emote sequences through System 4 (DQEmotionEngine).

DELETE FROM `dq_archetype`      WHERE `id` = 2;
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 2;

INSERT INTO `dq_archetype`
    (`id`, `name`,           `pattern`,    `total_beats`, `zone_id`, `level_min`, `level_max`, `enabled`, `appearance`)
VALUES
    (2,   'The Herb Seller', 'sequential', 4,              37,        1,           80,          1,         'humanoid,male');

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`,
     `mechanic`, `transition_type`, `transition_value`,
     `text_greeting`, `text_chase`,
     `emote_on_arrive`, `emote_on_complete`, `reward_pool`,
     `spawn_style`, `prop_entry`, `prop_count`, `prop_radius`,
     `emotion`, `emotion_end`, `text_on_accept`, `text_on_complete`)
VALUES
-- Beat 1: herb gather — hopeful old man with a bad back asks for help
(2, 1, 0, 37,
 'activate', 'quest_complete', 3,
 'Traveler! I hate to ask a stranger, but my back troubles me something fierce today. There are wild herbs growing just past the old fence — would you pick a handful? I can pay.',
 'Please — it would only take a moment!',
 0, 0, 'herb_seller_small',
 'approaches', 900100, 3, 12.0,
 'hopeful', 'grateful',
 'Bless you, friend. I knew you had a kind heart.',
 'Thank you. Truly.'),

-- Beat 2: witness — old man at his market stall, composed and businesslike
(2, 2, 0, 37,
 'witness', 'encounter_count', 1,
 'You catch sight of him at the market stall, calling out to buyers. The herbs you gathered are already bundled and priced.',
 '',
 0, 0, '',
 'waiting', 0, 0, 0.0,
 'solemn', '', '', ''),

-- Beat 3: courier task — grateful to see player again, sends them for a carved horse
(2, 3, 0, 37,
 'courier', 'encounter_count', 1,
 'Ah — you again. Good timing. Here, take this coin. There is a toy seller near the east gate. Buy me a small carved horse, would you? Child-sized.',
 'It is important. Please.',
 0, 0, '',
 'approaches', 0, 0, 0.0,
 'grateful', 'moved',
 'I will be here when you return.',
 'Perfect. Just perfect.'),

-- Beat 4: witness — old man gives the horse to his laughing child
(2, 4, 0, 37,
 'witness', 'encounter_count', 1,
 'The old man is crouched in the road, holding out the wooden horse. A small boy runs to him, laughing.',
 '',
 0, 0, 'herb_seller_final',
 'waiting', 0, 0, 0.0,
 'happy', 'joyful', '', '');
