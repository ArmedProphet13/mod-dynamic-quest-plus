-- DynamicQuests+ | Master Content Import
-- Safe to re-run at any time (content only — no schema changes).
-- Schema migrations are in updates/pending_world/ and must be applied separately (one-shot).
--
-- Active interactions:
--   Template  2 — Hungry Child      (tier 1, social)
--   Template 10 — Ileana Episode I   (tier 2, succubus)
--   Template 11 — Ileana Episode II  (tier 2, succubus)
--   Template 12 — Ileana Episode III (tier 2, succubus)
--
-- Archetypes (data-driven multi-beat engine — requires dq_archetype.sql schema):
--   Archetype 1 — Ileana (reserved; Ileana migrated to archetype engine in a future pass)
--   Archetype 2 — The Herb Seller  (4-beat Sequential, HiddenPurpose arc, zone 37)
--   Archetype 6 — The Sentinel's Report  (1-beat, witness, smoke test)
--   Archetype 7 — The Trader's Plea     (2-beat sequential, emotion arc, emotes)
--   Archetype 8 — The Urgent Call       (1-beat, run_up, urgent emotion, chase text)
--   Archetype 9 — The Stranger's Bargain (3-beat branching, from_shadow, choices)
--   Archetype 10 — The Herald's Trial   (4-beat, all mechanics, from_portal, reward pool)

-- ===== base/dq_chest_items.sql =====
-- DynamicQuests+ | Traveler's Treasure Chest items
--
-- Six soulbound loot chests delivered on event completion.
-- Right-click opens a loot window (ITEM_FLAG_HAS_LOOT = 4, same mechanism as
-- Blizzard's Satchel of Helpful Goods / Protectorate Treasure Cache).
-- The item inside is selected dynamically by DQChestOpener (PlayerScript),
-- class-appropriate and level-appropriate. No static loot table needed.
--
-- Entry layout:
--   900100 / 900101 / 900102  Armor Cache  (Quality 2/3/4 = green/blue/purple)
--   900103 / 900104 / 900105  Weapon Cache (Quality 2/3/4 = green/blue/purple)
--
-- displayid 12333 = classic wooden treasure chest (Small Chest, Battered Chest, Ironbound Locked
-- Chest — vanilla items confirmed in 3.3.5a client). Quality border (green/blue/purple outline)
-- rendered automatically by the client from the Quality field.
-- Flags = 4 (ITEM_FLAG_HAS_LOOT) enables right-click "Open" without any spell.

DELETE FROM `item_template` WHERE `entry` IN (900100, 900101, 900102, 900103, 900104, 900105);

INSERT INTO `item_template`
    (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`,
     `Quality`, `Flags`, `AllowableClass`, `AllowableRace`, `stackable`,
     `bonding`, `description`,
     `ScriptName`)
VALUES
    -- ── Armor Cache (green) ─────────────────────────────────────────────────
    (900100, 15, 0, -1, 'Traveler''s Armor Cache', 12333,
     2, 4, -1, -1, 1,
     1, 'Open to receive a piece of armor suited to your class and level. Bound to the finder.',
     ''),

    -- ── Armor Cache (blue) ──────────────────────────────────────────────────
    (900101, 15, 0, -1, 'Traveler''s Armor Cache', 12333,
     3, 4, -1, -1, 1,
     1, 'Open to receive quality armor suited to your class and level. Bound to the finder.',
     ''),

    -- ── Armor Cache (purple) ────────────────────────────────────────────────
    (900102, 15, 0, -1, 'Traveler''s Armor Cache', 12333,
     4, 4, -1, -1, 1,
     1, 'Open to receive exceptional armor worthy of a champion. Bound to the finder.',
     ''),

    -- ── Weapon Cache (green) ────────────────────────────────────────────────
    (900103, 15, 0, -1, 'Traveler''s Weapon Cache', 12333,
     2, 4, -1, -1, 1,
     1, 'Open to receive a weapon suited to your class and level. Bound to the finder.',
     ''),

    -- ── Weapon Cache (blue) ─────────────────────────────────────────────────
    (900104, 15, 0, -1, 'Traveler''s Weapon Cache', 12333,
     3, 4, -1, -1, 1,
     1, 'Open to receive a quality weapon suited to your class and level. Bound to the finder.',
     ''),

    -- ── Weapon Cache (purple) ───────────────────────────────────────────────
    (900105, 15, 0, -1, 'Traveler''s Weapon Cache', 12333,
     4, 4, -1, -1, 1,
     1, 'Open to receive an exceptional weapon worthy of a champion. Bound to the finder.',
     '');


-- ===== interactions/hungry_child.sql =====
-- DynamicQuests+ | Interaction: "Hungry Child"
-- Theme: social | Mechanic: hungry_child | Tier: 1 | Level: 1-80
--
-- A child NPC asks the player for food. Available in capital cities.
-- Item 33052 = Freshly Baked Bread (sold by innkeepers and food vendors).
-- Emotes: CHEER(4) on give, SIT(1) on fetch-accept, CRY(22) on refuse.

-- Gossip window text for the Hungry Child interaction.
DELETE FROM `npc_text` WHERE `ID` = 9000002;
INSERT INTO `npc_text` (`ID`, `text0_0`, `Probability0`) VALUES
(9000002, 'A small child pulls at your sleeve, eyes wide with hunger. "Please... do you have any bread? The innkeeper sells it, but I have nothing left."', 1.0);

DELETE FROM `dq_interaction_template` WHERE `id` = 2;
INSERT INTO `dq_interaction_template`
    (`id`, `name`, `theme`, `npc_tags_req`, `npc_tags_excl`,
     `zone_type`, `level_min`, `level_max`, `prereq_gold`,
     `context_tags`, `tier`, `mechanic_type`, `rarity_weight`,
     `combat_hp_pct`, `combat_dmg_pct`, `gossip_npc_text_id`,
     `zone_faction`)
VALUES
    (2, 'Hungry Child', 'social',
     'child,humanoid,civilian',
     'demon,undead,hostile,horde',
     'capital', 1, 80, 0,
     'traveling,idle,exploring', 1, 'gossip_exchange', 12,
     0, 0, 9000002,
     'alliance');

DELETE FROM `dq_interaction_choice` WHERE `template_id` = 2;
INSERT INTO `dq_interaction_choice`
    (`template_id`, `display_order`, `choice_text`, `prereq_item_id`,
     `outcome_type`, `outcome_data`, `emote_on_select`)
VALUES
    -- Give bread immediately — only appears if player already has one
    (2, 0, 'Here you go, little one. Take this.', 33052,
     'reward', 'cost:item:33052,pool:charity_low', 4),

    -- Fetch: go find bread and return to the child
    (2, 1, 'I''ll find you something. Wait here.', 0,
     'fetch', 'item:33052,pool:charity_low', 1),

    -- Refuse: child cries
    (2, 2, 'I''m sorry, I have nothing to spare.', 0,
     'deny', '', 22);

-- Reward pool for charity — small but meaningful at every level bracket
DELETE FROM `dq_reward_pool` WHERE `pool_name` = 'charity_low';
INSERT INTO `dq_reward_pool`
    (`pool_name`, `level_min`, `level_max`, `item_entry`,
     `gold_min_copper`, `gold_max_copper`, `xp_amount`, `weight`)
VALUES
    ('charity_low',  1, 29, 0,  5000,  15000,  500, 10),
    ('charity_low', 30, 59, 0, 10000,  30000, 1500, 10),
    ('charity_low', 60, 80, 0, 20000,  50000,    0, 10);

-- Text variants: greeting (child runs up) and chase (child follows player)
DELETE FROM `dq_text_variant` WHERE `template_id` = 2;
INSERT INTO `dq_text_variant` (`template_id`, `variant_type`, `text`, `weight`) VALUES
    (2, 'greeting', '"Mister, missus, please... do you have any food?"',                    10),
    (2, 'greeting', 'A small hand grabs your cloak. The child looks up, hollow-cheeked.',   10),
    (2, 'greeting', '"I haven''t eaten since yesterday. Please..."',                         9),
    (2, 'greeting', 'The child stands in your path, too tired to move out of the way.',     8),
    (2, 'greeting', '"You look kind. Are you kind? Can you help me?"',                       7),
    (2, 'chase',    '"Please don''t go. I''m so hungry."',                                   10),
    (2, 'chase',    'The child trots after you, arms outstretched.',                         10),
    (2, 'chase',    '"I won''t bother you long. I promise."',                                8),
    (2, 'chase',    '"Just a piece of bread. Just one."',                                    7);


-- ===== interactions/ileana_episode1.sql =====
-- DynamicQuests+ Module | Ileana — Episode I: The First Offer
-- Theme: demon | Mechanic: succubus | Tier: 2 | Level: 10-80
-- Costs: 70% gold (capped at level×1g). Reward: 1× Blue Cache.
-- Fight reward: 1× Blue Weapon Cache.

-- ── Dialog pages (npc_text) ──────────────────────────────────────────────────

DELETE FROM `npc_text` WHERE `ID` IN (9001010, 9001011, 9001012);

-- Page 1: The portal tears open. She introduces herself.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001010,
'$B$BThe portal tears open without warning. No ritual. No invitation. Violet fire bleeds through the gap - like something on the other side has been pressing against it for a long time, waiting for exactly this moment.$B$B"Oh, don''t reach for that. We both know how that ends, and I''d rather not begin our relationship with something so unpleasant."$B$B');

-- Page 2: The pitch.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001011,
'$B$B"You''ve been working hard. I''ve watched. All that struggle - all that effort - and still not quite enough, is it? There''s always something just out of reach."$B$BShe tilts her head.$B$B"I can change that. Right now. No temples, no gods, no years of crawling toward something that keeps moving. Just power - real power - and it''s yours."$B$B');

-- Page 3 (final): The question.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001012,
'$B$B"All I ask is something small in return. Something you already carry. Something you won''t even miss."$B$BThe portal pulses behind her. Waiting.$B$B"So. Do we have a deal?"$B$B');

-- ── Interaction template ──────────────────────────────────────────────────────

DELETE FROM `dq_interaction_template`  WHERE `id` = 10;
DELETE FROM `dq_interaction_choice`    WHERE `template_id` = 10;

INSERT INTO `dq_interaction_template`
    (`id`, `name`, `theme`, `npc_tags_req`, `npc_tags_excl`,
     `zone_type`, `level_min`, `level_max`, `prereq_gold`,
     `context_tags`, `tier`, `mechanic_type`, `rarity_weight`,
     `combat_hp_pct`, `combat_dmg_pct`, `gossip_npc_text_id`,
     `combat_hp_ratio`, `combat_dmg_ratio`, `zone_faction`)
VALUES
    (10, 'Ileana — Episode I', 'demon', 'demon', 'boss,child,vendor',
     'outdoor', 10, 80, 0,
     'any', 2, 'succubus', 5,
     0, 0, 9001010,
     1.8, 0.07, 'any');

-- Choices are handled entirely by DQ_SuccubusAI gossip logic.
-- These rows exist as metadata only; DQMgr reads choiceIndex from gossip action.
INSERT INTO `dq_interaction_choice`
    (`template_id`, `display_order`, `choice_text`,
     `prereq_item_id`, `outcome_type`, `outcome_data`, `emote_on_select`)
VALUES
    (10, 0, '...done.',                  0, 'reward', 'succubus_accept_ep1', 0),
    (10, 1, 'Never. I don''t deal with demons.', 0, 'combat', 'succubus_fight_ep1', 0);

-- ── Text variants (ambient lines during approach) ─────────────────────────────

DELETE FROM `dq_text_variant` WHERE `template_id` = 10;
INSERT INTO `dq_text_variant` (`template_id`, `variant_type`, `text`, `weight`) VALUES
    (10, 'greeting', '*She steps through. Deliberate. Patient. Like she''s done this before.*',         5),
    (10, 'greeting', '*Something tears the air ahead of you. Then - silence. Then her.*',              5),
    (10, 'greeting', '*The light changes first. Then the sound. Then she is simply there.*',           5),
    (10, 'chase',    '"Don''t run. I haven''t even made my offer yet."',                               5),
    (10, 'chase',    '"You''re more interesting when you''re frightened. Marginally."',                4),
    (10, 'chase',    '"The portal follows you. It''s not patient. Neither am I."',                     4);


-- ===== interactions/ileana_episode2.sql =====
-- DynamicQuests+ Module | Ileana — Episode II: The Price
-- Theme: demon | Mechanic: succubus | Tier: 2 | Level: 10-80
-- Costs: 70% gold (capped at level×1g) + all potions.
-- Reward: 1× Blue Armor Cache + 1× Blue Weapon Cache (both accept and fight).

-- ── Dialog pages (npc_text) ──────────────────────────────────────────────────

DELETE FROM `npc_text` WHERE `ID` IN (9001020, 9001021, 9001022, 9001023);

-- Page 1: She's back. The real pitch.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001020,
'$B$BThe portal opens. Same violet fire. Same silence before it, like the air knew it was coming and decided not to warn you.$B$B"You thought about it. I know you did. That''s why I''m back."$B$BShe tilts her head.$B$B"You''ve been watching others rise. People with less than you — less discipline, less sacrifice — and somehow they keep getting further ahead. That bothers you. It should."$B$B"What I''m offering doesn''t cost you anything you''re using. Think of it less as a deal and more as correcting an imbalance. You''ve already paid more than most. I''m just here to make sure you get something back."$B$B');

-- Page 2 (final): The close.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001021,
'$B$B"Nothing you''ll miss!"$B$B"You won''t even know what you gave. That''s the beauty of it."$B$B"Power without the weight of wondering whether you deserve it. Doesn''t that sound like relief?"$B$B');

-- ── Interaction template ──────────────────────────────────────────────────────

DELETE FROM `dq_interaction_template`  WHERE `id` = 11;
DELETE FROM `dq_interaction_choice`    WHERE `template_id` = 11;

INSERT INTO `dq_interaction_template`
    (`id`, `name`, `theme`, `npc_tags_req`, `npc_tags_excl`,
     `zone_type`, `level_min`, `level_max`, `prereq_gold`,
     `context_tags`, `tier`, `mechanic_type`, `rarity_weight`,
     `combat_hp_pct`, `combat_dmg_pct`, `gossip_npc_text_id`,
     `combat_hp_ratio`, `combat_dmg_ratio`, `zone_faction`)
VALUES
    (11, 'Ileana — Episode II', 'demon', 'demon', 'boss,child,vendor',
     'outdoor', 10, 80, 0,
     'any', 2, 'succubus', 5,
     0, 0, 9001020,
     1.8, 0.07, 'any');

INSERT INTO `dq_interaction_choice`
    (`template_id`, `display_order`, `choice_text`,
     `prereq_item_id`, `outcome_type`, `outcome_data`, `emote_on_select`)
VALUES
    (11, 0, 'Deal.',                        0, 'reward', 'succubus_accept_ep2', 0),
    (11, 1, 'Don''t trust demons. Fight.',  0, 'combat', 'succubus_fight_ep2',  0);

-- ── Text variants ─────────────────────────────────────────────────────────────

DELETE FROM `dq_text_variant` WHERE `template_id` = 11;
INSERT INTO `dq_text_variant` (`template_id`, `variant_type`, `text`, `weight`) VALUES
    (11, 'greeting', '*She''s back. The same violet fire. The same patience.*',                        5),
    (11, 'greeting', '*The air grows still before you hear anything. Then her voice.*',                5),
    (11, 'greeting', '*She steps through as if the portal is a doorway she owns.*',                   5),
    (11, 'chase',    '"Running again. That''s consistent, at least."',                                 5),
    (11, 'chase',    '"I came back. That should tell you something."',                                 5),
    (11, 'chase',    '"We aren''t finished. We''ve barely started."',                                  4);


-- ===== interactions/ileana_episode3.sql =====
-- DynamicQuests+ Module | Ileana — Episode III: No More Fractions
-- Theme: demon | Mechanic: succubus | Tier: 2 | Level: 10-80
-- Costs: 70% gold + 4-hour Resurrection Sickness debuff (soul debt).
-- Reward: 1× Purple Weapon Cache (both accept and fight).

-- ── Dialog pages (npc_text) ──────────────────────────────────────────────────

DELETE FROM `npc_text` WHERE `ID` IN (9001030, 9001031, 9001032);

-- Page 1: Control taken before a word is spoken.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001030,
'$B$BThe portal tears open violently. Violet fire spills across the ground, swallowing sound with it. Before you can move, she lifts two fingers.$B$BAnd suddenly -$B$B Nothing.$B$BYour body locks in place. Not pain. Not paralysis. Worse. Control simply leaves you.$B$BShe steps through the portal slowly, savouring the realisation in your eyes.$B$B"There it is."$B$B');

-- Page 2: The truth of it.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001031,
'$B$BShe approaches, close enough now that the air tastes sweet and burning.$B$B"The first time, you feared the cost. The second time, you feared losing yourself."$B$BHer fingers brush against your chin, forcing your gaze upward.$B$B"But now? Now you''re afraid this is real."$B$BShe studies you for a moment. Almost tenderly.$B$B"You''ve spent your whole life clawing power from a world that rewards cruelty and calls it virtue. And still you hesitate when power finally offers itself willingly."$B$BThe violet fire rises behind her, stretching upward like enormous wings.$B$B"No more fractions. No more harmless little trades."$B$B"I am offering MORE."$B$B');

-- Page 3 (final): The last offer. The real price.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001032,
'$B$B"Strength beyond mortal measure. The kind heroes beg for at dying altars. The kind kings drown nations trying to possess."$B$BThen, softly:$B$B"I will not take a memory."$B$BThe room goes still.$B$B"I will take the part of you that would have refused me."$B$BShe leans close enough to whisper.$B$B"And when the world finally kneels before you... you will never remember why that should horrify you."$B$B');

-- ── Interaction template ──────────────────────────────────────────────────────

DELETE FROM `dq_interaction_template`  WHERE `id` = 12;
DELETE FROM `dq_interaction_choice`    WHERE `template_id` = 12;

INSERT INTO `dq_interaction_template`
    (`id`, `name`, `theme`, `npc_tags_req`, `npc_tags_excl`,
     `zone_type`, `level_min`, `level_max`, `prereq_gold`,
     `context_tags`, `tier`, `mechanic_type`, `rarity_weight`,
     `combat_hp_pct`, `combat_dmg_pct`, `gossip_npc_text_id`,
     `combat_hp_ratio`, `combat_dmg_ratio`, `zone_faction`)
VALUES
    (12, 'Ileana — Episode III', 'demon', 'demon', 'boss,child,vendor',
     'outdoor', 10, 80, 0,
     'any', 2, 'succubus', 5,
     0, 0, 9001030,
     2.5, 0.10, 'any');

INSERT INTO `dq_interaction_choice`
    (`template_id`, `display_order`, `choice_text`,
     `prereq_item_id`, `outcome_type`, `outcome_data`, `emote_on_select`)
VALUES
    (12, 0, 'Take anything. I''m yours.',               0, 'reward', 'succubus_accept_ep3', 0),
    (12, 1, 'This time I won''t fall for your tricks.', 0, 'combat', 'succubus_fight_ep3',  0);

-- ── Text variants ─────────────────────────────────────────────────────────────

DELETE FROM `dq_text_variant` WHERE `template_id` = 12;
INSERT INTO `dq_text_variant` (`template_id`, `variant_type`, `text`, `weight`) VALUES
    (12, 'greeting', '*The portal opens before you hear it. That''s new.*',                           5),
    (12, 'greeting', '*This time, there is no pause. She is simply there. Already.*',                 5),
    (12, 'greeting', '*Something is different. The air doesn''t warn you this time.*',                5),
    (12, 'chase',    '"You can''t run from this one."',                                               5),
    (12, 'chase',    '"You''ve already made your choice. You just haven''t said it yet."',            5),
    (12, 'chase',    '"The third time. I don''t come a fourth."',                                     5);


-- ===== base/dq_herb_seller.sql =====
-- DynamicQuests+ | The Herb Seller — HiddenPurpose archetype (4-beat Sequential)
--
-- Story arc:
--   Beat 1 (activate) — NPC asks player to gather 3 herb bundles nearby.
--   Beat 2 (witness)  — Player sees him selling the same herbs at the market.
--   Beat 3 (courier)  — NPC gives coin, asks to buy a carved toy horse.
--   Beat 4 (witness)  — Player sees him give the toy to a laughing child.
--
-- Zone 37 = Hillsbrad Foothills. level_min=1, level_max=80.
-- appearance 'humanoid,male' — engine picks a zone-appropriate elderly male model.

DELETE FROM `dq_archetype`      WHERE `id` = 2;
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 2;

INSERT INTO `dq_archetype`
    (`id`, `name`,           `pattern`,    `total_beats`, `zone_id`, `level_min`, `level_max`, `enabled`, `appearance`)
VALUES
    (2,   'The Herb Seller', 'sequential', 4,              37,        1,           80,          1,         'humanoid,male');

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`,
     `mechanic`,  `transition_type`,  `transition_value`,
     `text_greeting`, `text_chase`,
     `reward_pool`,
     `spawn_style`, `prop_entry`, `prop_count`, `prop_radius`,
     `emotion`, `emotion_end`, `text_on_accept`, `text_on_complete`)
VALUES
-- Beat 1: herb gather — hopeful old man with a bad back asks for help
(2, 1, 0, 37,
 'activate', 'quest_complete', 3,
 'Traveler! I hate to ask a stranger, but my back troubles me something fierce today. There are wild herbs growing just past the old fence — would you pick a handful? I can pay.',
 'Please — it would only take a moment!',
 'herb_seller_small',
 'approaches', 900100, 3, 12.0,
 'pleading', 'grateful',
 'Bless you, friend. I knew you had a kind heart.',
 'Thank you. Truly.'),

-- Beat 2: witness — old man at his market stall, businesslike and composed
(2, 2, 0, 37,
 'witness', 'encounter_count', 1,
 'You catch sight of him at the market stall, calling out to buyers. The herbs you gathered are already bundled and priced.',
 '',
 '',
 'waiting', 0, 0, 0.0,
 'neutral', '', '', ''),

-- Beat 3: courier task — grateful to see player again, sends them for a toy
(2, 3, 0, 37,
 'courier', 'encounter_count', 1,
 'Ah — you again. Good timing. Here, take this coin. There is a toy seller near the east gate. Buy me a small carved horse, would you? Child-sized.',
 'It is important. Please.',
 '',
 'approaches', 0, 0, 0.0,
 'grateful', '',
 'I will be here when you return.',
 'Perfect. Just perfect.'),

-- Beat 4: witness — old man gives the horse to his child, pure joy
(2, 4, 0, 37,
 'witness', 'encounter_count', 1,
 'The old man is crouched in the road, holding out the wooden horse. A small boy runs to him, laughing.',
 '',
 'herb_seller_final',
 'waiting', 0, 0, 0.0,
 'happy', '', '', '');

-- ===== base/dq_gameobject_template.sql =====
-- DQ+ interactable prop GOs (entries 900100-900102, type 22 GOOBER, ScriptName DQ_PropGO).
-- Display IDs: Goldthorn herb (698), Excavation Supply Crate (31), Noggle's Satchel (323).

DELETE FROM `gameobject_template` WHERE `entry` IN (900100, 900101, 900102);

INSERT INTO `gameobject_template`
    (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`,
     `Data0`, `Data1`, `Data2`, `Data3`, `Data4`, `Data5`, `Data6`, `Data7`,
     `Data8`, `Data9`, `Data10`, `Data11`, `Data12`, `Data13`, `Data14`, `Data15`,
     `Data16`, `Data17`, `Data18`, `Data19`, `Data20`, `Data21`, `Data22`, `Data23`,
     `AIName`, `ScriptName`, `VerifiedBuild`)
VALUES
(900100, 22, 698,  'DQ Herb Bundle',   '', '', '', 1.0,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, '', 'DQ_PropGO', 0),
(900101, 22, 31,   'DQ Supply Crate',  '', '', '', 1.0,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, '', 'DQ_PropGO', 0),
(900102, 22, 323,  'DQ Fallen Pack',   '', '', '', 1.0,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, '', 'DQ_PropGO', 0);

-- DQ Runic Beacon — used by archetype 10 beat 3 (Herald's Trial, activate mechanic).
-- displayId 31 = crate model, visual placeholder; swap for a torch/fire displayId later.

DELETE FROM `gameobject_template` WHERE `entry` = 900103;

INSERT INTO `gameobject_template`
    (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`,
     `Data0`, `Data1`, `Data2`, `Data3`, `Data4`, `Data5`, `Data6`, `Data7`,
     `Data8`, `Data9`, `Data10`, `Data11`, `Data12`, `Data13`, `Data14`, `Data15`,
     `Data16`, `Data17`, `Data18`, `Data19`, `Data20`, `Data21`, `Data22`, `Data23`,
     `AIName`, `ScriptName`, `VerifiedBuild`)
VALUES
(900103, 22, 31, 'DQ Runic Beacon', '', '', '', 0.8,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, '', 'DQ_PropGO', 0);

-- ===== base/dq_test_archetypes.sql =====
-- DynamicQuests+ | Test Archetypes 6-10
-- Progressive test coverage: smoke → sequential → spawn styles → branching → full system.
-- All zone_id = 0 (any zone). Levels 1-80.

-- Reward pools -----------------------------------------------------------------

DELETE FROM `dq_reward_pool` WHERE `pool_name` IN ('trader_small','urgent_reward','stranger_reward','herald_cache');

INSERT INTO `dq_reward_pool`
    (`pool_name`, `level_min`, `level_max`, `item_entry`, `gold_min_copper`, `gold_max_copper`, `xp_amount`, `weight`)
VALUES
    -- trader_small (archetype 7): green armor cache
    ('trader_small',      1,  29, 900100,  3000,  10000,  400, 10),
    ('trader_small',     30,  59, 900100,  8000,  25000, 1200, 10),
    ('trader_small',     60,  80, 900100, 15000,  40000,    0, 10),
    -- urgent_reward (archetype 8): green armor cache
    ('urgent_reward',     1,  29, 900100,  5000,  15000,  600, 10),
    ('urgent_reward',    30,  59, 900100, 12000,  30000, 1800, 10),
    ('urgent_reward',    60,  80, 900100, 25000,  60000,    0, 10),
    -- stranger_reward (archetype 9 accept path): blue armor cache
    ('stranger_reward',   1,  29, 900101,  8000,  20000,  800, 10),
    ('stranger_reward',  30,  59, 900101, 18000,  40000, 2000, 10),
    ('stranger_reward',  60,  80, 900101, 35000,  70000,    0, 10),
    -- herald_cache (archetype 10 full trial): purple armor cache
    ('herald_cache',      1,  29, 900102, 10000,  25000, 1000, 10),
    ('herald_cache',     30,  59, 900102, 20000,  50000, 2500, 10),
    ('herald_cache',     60,  80, 900102, 40000,  80000,    0, 10);

-- Archetype 6: The Sentinel's Report -------------------------------------------
-- Smoke test: approaches spawn, witness, curious→content, talk+wave emotes.

DELETE FROM `dq_archetype`      WHERE `id` = 6;
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 6;

INSERT INTO `dq_archetype`
    (`id`, `name`, `pattern`, `total_beats`, `zone_id`, `level_min`, `level_max`, `enabled`, `appearance`)
VALUES
    (6, 'The Sentinel''s Report', 'sequential', 1, 0, 1, 80, 1, 'humanoid,male');

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`,
     `mechanic`, `transition_type`, `transition_value`,
     `text_greeting`, `text_chase`,
     `reward_pool`,
     `spawn_style`, `prop_entry`, `prop_count`, `prop_radius`,
     `emotion`, `emotion_end`, `text_on_accept`, `text_on_complete`)
VALUES
(6, 1, 0, 0,
 'witness', 'quest_complete', 1,
 'A word, traveler. The road ahead is clear. Safe travels.',
 'Hold on — just a moment.',
 '',
 'approaches', 0, 0, 0.0,
 'neutral', '',
 'Understood.',
 'The sentinel nods and returns to his post.');

-- Archetype 7: The Trader's Plea -----------------------------------------------
-- 2-beat sequential: roadside witness (anxious) → waiting courier (hopeful) + reward.
-- Tests: beat-to-beat transition, emotion arc, emoteOnArrive + emoteOnComplete.

DELETE FROM `dq_archetype`      WHERE `id` = 7;
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 7;

INSERT INTO `dq_archetype`
    (`id`, `name`, `pattern`, `total_beats`, `zone_id`, `level_min`, `level_max`, `enabled`, `appearance`)
VALUES
    (7, 'The Trader''s Plea', 'sequential', 2, 0, 1, 80, 1, 'humanoid,male');

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`,
     `mechanic`, `transition_type`, `transition_value`,
     `text_greeting`, `text_chase`,
     `reward_pool`,
     `spawn_style`, `prop_entry`, `prop_count`, `prop_radius`,
     `emotion`, `emotion_end`, `text_on_accept`, `text_on_complete`)
VALUES
(7, 1, 0, 0,
 'witness', 'quest_complete', 1,
 'Over here! Bandits hit the south road. I need someone reliable.',
 'Please — it will only take a moment!',
 '',
 'roadside', 0, 0, 0.0,
 'fearful', 'victorious',
 'Tell me what you need.',
 'One more thing...'),
(7, 2, 0, 0,
 'courier', 'encounter_count', 1,
 'Take this coin to the innkeeper at the crossroads. Tell her Aldric sent you.',
 'I have nowhere else to turn.',
 'trader_small',
 'waiting', 0, 0, 0.0,
 'pleading', 'grateful',
 'Consider it done.',
 'Thank you, friend. Light willing, I''ll reach the city by dawn.');

-- Archetype 8: The Urgent Call -------------------------------------------------
-- 1-beat courier, run_up spawn, urgent emotion, exclamation+salute emotes, chase text.

DELETE FROM `dq_archetype`      WHERE `id` = 8;
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 8;

INSERT INTO `dq_archetype`
    (`id`, `name`, `pattern`, `total_beats`, `zone_id`, `level_min`, `level_max`, `enabled`, `appearance`)
VALUES
    (8, 'The Urgent Call', 'sequential', 1, 0, 1, 80, 1, 'humanoid,male');

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`,
     `mechanic`, `transition_type`, `transition_value`,
     `text_greeting`, `text_chase`,
     `reward_pool`,
     `spawn_style`, `prop_entry`, `prop_count`, `prop_radius`,
     `emotion`, `emotion_end`, `text_on_accept`, `text_on_complete`)
VALUES
(8, 1, 0, 0,
 'courier', 'encounter_count', 1,
 'Traveler — no time. Take this seal to the garrison. North road. Double time.',
 'Stop! Lives depend on this!',
 'urgent_reward',
 'run_up', 0, 0, 0.0,
 'angry', 'happy',
 'I''ll get it there.',
 'The messenger salutes sharply and sprints away.');

-- Archetype 9: The Stranger's Bargain ------------------------------------------
-- 3-beat branching: from_shadow arrival, 2 gossip choices.
-- Choice 0 → beat 2 (accepted, reward). Choice 1 → beat 3 (refused, no reward).
-- Routing: nextBeat = choiceIndex + 2 (AC branching pattern math).

DELETE FROM `dq_archetype`      WHERE `id` = 9;
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 9;

INSERT INTO `dq_archetype`
    (`id`, `name`, `pattern`, `total_beats`, `zone_id`, `level_min`, `level_max`, `enabled`, `appearance`)
VALUES
    (9, 'The Stranger''s Bargain', 'branching', 3, 0, 1, 80, 1, 'humanoid,male');

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`,
     `mechanic`, `transition_type`, `transition_value`,
     `text_greeting`, `text_chase`,
     `reward_pool`,
     `spawn_style`, `prop_entry`, `prop_count`, `prop_radius`,
     `emotion`, `emotion_end`, `text_on_accept`, `text_on_complete`)
VALUES
(9, 1, 0, 0,
 'courier', 'choice', 1,
 'You look capable. I have a job — pays well, no questions asked. Interested?',
 'Think carefully. Opportunities like this don''t come twice.',
 '',
 'from_shadow', 0, 0, 0.0,
 'neutral', '',
 'Tell me more.',
 'Not my problem.'),
(9, 2, 0, 0,
 'witness', 'quest_complete', 1,
 'Smart choice. Here is your cut. Don''t spend it all in one place.',
 '',
 'stranger_reward',
 'waiting', 0, 0, 0.0,
 'grateful', '',
 'Pleasure doing business.',
 'The stranger vanishes as quickly as he appeared.'),
(9, 3, 0, 0,
 'witness', 'quest_complete', 1,
 'Your loss, traveler. Someone else will take the coin.',
 '',
 '',
 'waiting', 0, 0, 0.0,
 'angry', '',
 'Your choice.',
 'The stranger shrugs and disappears into the shadows.');

-- Archetype 10: The Herald's Trial ---------------------------------------------
-- 4-beat sequential: all 4 implemented mechanics in sequence.
-- Beat 1 witness (from_portal) → Beat 2 kill 3 → Beat 3 activate beacon → Beat 4 courier + reward.

DELETE FROM `dq_archetype`      WHERE `id` = 10;
DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 10;

INSERT INTO `dq_archetype`
    (`id`, `name`, `pattern`, `total_beats`, `zone_id`, `level_min`, `level_max`, `enabled`, `appearance`)
VALUES
    (10, 'The Herald''s Trial', 'sequential', 4, 0, 1, 80, 1, 'humanoid,male');

INSERT INTO `dq_archetype_beat`
    (`archetype_id`, `beat_number`, `display_id`, `zone_id`,
     `mechanic`, `transition_type`, `transition_value`,
     `text_greeting`, `text_chase`,
     `reward_pool`,
     `spawn_style`, `prop_entry`, `prop_count`, `prop_radius`,
     `emotion`, `emotion_end`, `text_on_accept`, `text_on_complete`)
VALUES
(10, 1, 0, 0,
 'witness', 'quest_complete', 1,
 'Champion. Darkness stirs nearby. I bring a trial — three tasks. Will you face them?',
 'Do not walk away. The enemy does not wait.',
 '',
 'from_portal', 0, 0, 0.0,
 'angry', 'victorious',
 'I am ready.',
 'Then prove it. Slay three of the enemy.'),
(10, 2, 0, 0,
 'kill', 'encounter_count', 3,
 'Three enemies must fall. Any who threaten the innocent will do.',
 'Do not stop. Three must die.',
 '',
 'approaches', 0, 0, 0.0,
 'victorious', 'happy',
 'Understood.',
 'Good. Now the beacon.'),
(10, 3, 0, 0,
 'activate', 'quest_complete', 1,
 'A beacon has been placed nearby. Find it and activate it.',
 'The beacon awaits. Do not delay.',
 '',
 'approaches', 900103, 1, 25.0,
 'neutral', 'happy',
 'I will find it.',
 'The signal is lit. One final task.'),
(10, 4, 0, 0,
 'courier', 'encounter_count', 1,
 'You have proven yourself, champion. The trial is complete. Take your reward.',
 'Do not go — the reward is yours.',
 'herald_cache',
 'waiting', 0, 0, 0.0,
 'pleading', 'grateful',
 'Well fought.',
 'The herald gives a firm salute and steps back through the portal.');
