-- DynamicQuests+ | Interaction: "Hungry Child"
-- Theme: social | Mechanic: gossip_exchange | Tier: 1 | Level: 1-80
--
-- A child NPC asks the player for food. Available in capital cities.
-- Item 33052 = Freshly Baked Bread (sold by innkeepers and food vendors).
-- Emotes: CHEER(4) on give, SIT(1) on fetch-accept, CRY(22) on refuse.

-- Gossip window header (DELETE + INSERT so text is always up to date)
DELETE FROM `npc_text` WHERE `ID` = 9000002;
INSERT INTO `npc_text` (`ID`, `text0_0`, `em0_0`) VALUES
(9000002, 'A small child pulls at your sleeve, eyes wide with hunger. "Please... do you have any bread? The innkeeper sells it, but I have nothing left."', 0);

DELETE FROM `dq_interaction_template` WHERE `id` = 2;
INSERT INTO `dq_interaction_template`
    (`id`, `name`, `theme`, `npc_tags_req`, `npc_tags_excl`,
     `zone_type`, `level_min`, `level_max`, `prereq_gold`,
     `context_tags`, `tier`, `mechanic_type`, `rarity_weight`,
     `combat_hp_pct`, `combat_dmg_pct`, `gossip_npc_text_id`,
     `combat_hp_ratio`, `combat_dmg_ratio`, `zone_faction`)
VALUES
    (2, 'Hungry Child', 'social',
     'child,humanoid,civilian',
     'demon,undead,hostile,horde',
     'capital', 1, 80, 0,
     'traveling,idle,exploring', 1, 'gossip_exchange', 12,
     0, 0, 9000002,
     0.0, 0.0, 'alliance');

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
