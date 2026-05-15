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
