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
