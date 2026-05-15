-- DynamicQuests+ Module | Ileana — Episode I: The First Offer
-- Theme: demon | Mechanic: succubus | Tier: 2 | Level: 10-80
-- Costs: 70% gold (capped at level×1g). Reward: 1× Blue Cache.
-- Fight reward: 1× Blue Weapon Cache.

-- ── Dialog pages (npc_text) ──────────────────────────────────────────────────

DELETE FROM `npc_text` WHERE `ID` IN (9001010, 9001011, 9001012);

-- Page 1: The portal tears open. She introduces herself.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES (9001010, '$B$BThe portal tears open without warning. No ritual. No invitation. Violet fire bleeds through the gap - like something on the other side has been pressing against it for a long time, waiting for exactly this moment.$B$B"Oh, don''t reach for that. We both know how that ends, and I''d rather not begin our relationship with something so unpleasant."$B$B');

-- Page 2: The pitch.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES (9001011, '$B$B"You''ve been working hard. I''ve watched. All that struggle - all that effort - and still not quite enough, is it? There''s always something just out of reach."$B$BShe tilts her head.$B$B"I can change that. Right now. No temples, no gods, no years of crawling toward something that keeps moving. Just power - real power - and it''s yours."$B$B');

-- Page 3 (final): The question.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES (9001012, '$B$B"All I ask is something small in return. Something you already carry. Something you won''t even miss."$B$BThe portal pulses behind her. Waiting.$B$B"So. Do we have a deal?"$B$B');

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
