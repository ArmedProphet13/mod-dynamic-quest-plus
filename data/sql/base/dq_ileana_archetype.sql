-- DQ+ Archetype: Ileana the Bargainer (id=4)
-- 9-beat sequential arc (3 episodes × 3 beats each).
-- Beat 3/6/9 are branching choice beats (accept=cost, fight=TODO: DQ_BEAT_HOSTILE).
-- Episode gating is automatic via character_dq_sequences.current_beat.
-- spawn_style='from_portal' — succubus model 2575.
--
-- TODO (deferred, doesn't fit current engine):
--   soul_debt_hours (persistent aura tracking)
--   DQ_BEAT_HOSTILE mechanic for choice=fight path (beat 3/6/9 choice 1)
--   Multi-page gossip for exposition beats (text_gossip_page2/page3 columns)

DELETE FROM `dq_archetype_beat` WHERE `archetype_id` = 4;
DELETE FROM `dq_archetype`      WHERE `id` = 4;

INSERT INTO `dq_archetype`
    (`id`, `name`, `pattern`, `total_beats`, `zone_id`, `level_min`, `level_max`,
     `enabled`, `appearance`, `required_tags`, `excluded_tags`)
VALUES
    (4, 'Ileana the Bargainer', 'sequential', 9, 0, 20, 80,
     1, 'succubus,demon,female',
     'outdoor,unsettled',
     'city,sanctuary,instance');

-- ── Episode 1: The Offer ──────────────────────────────────────────────────────

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
    -- Beat 1: First encounter — Ileana introduces herself
    (4, 1, 2575, 0, 'witness',
     'encounter_count', 1,
     'You... you feel it too, don''t you? That hollow ache behind your ribs.',
     'Do not walk away. I know what you want.',
     0, 0,
     '', 'from_portal',
     0, 0, 0.0,
     'seductive', '',
     'I''m listening.',
     '',
     0, 0, 0,
     0, 0),

    -- Beat 2: She makes the proposition explicit
    (4, 2, 2575, 0, 'witness',
     'encounter_count', 1,
     'I offer power. Clarity. The world bends to those bold enough to take what they deserve.',
     'You came back. Good.',
     0, 0,
     '', 'from_portal',
     0, 0, 0.0,
     'seductive', '',
     'What is the price?',
     '',
     0, 0, 0,
     0, 0),

    -- Beat 3: The binding — player pays gold + HP, receives a boon
    (4, 3, 2575, 0, 'witness',
     'quest_complete', 1,
     'A trifle. Your gold means nothing to what I give you. And a drop of life-force... barely a bruise.',
     'You cannot un-hear this offer.',
     0, 0,
     'ileana_ep1_reward', 'from_portal',
     0, 0, 0.0,
     'menacing', 'satisfied',
     'So be it.',
     'She smiles. The deal is struck.',
     0, 0, 0,
     70, 20),

-- ── Episode 2: The Debt ───────────────────────────────────────────────────────

    -- Beat 4: Ileana returns; the consequences of the first deal manifest
    (4, 4, 2575, 0, 'witness',
     'encounter_count', 1,
     'Did you enjoy your gift? Of course you did. But gifts have weight, darling.',
     'You cannot hide from what you have already agreed to.',
     0, 0,
     '', 'from_portal',
     0, 0, 0.0,
     'menacing', '',
     'What do you want now?',
     '',
     0, 0, 0,
     0, 0),

    -- Beat 5: She reveals the escalating cost
    (4, 5, 2575, 0, 'witness',
     'encounter_count', 1,
     'Another favour. Nothing you cannot spare. Surely.',
     'Surely.',
     0, 0,
     '', 'from_portal',
     0, 0, 0.0,
     'menacing', '',
     'Name your price.',
     '',
     0, 0, 0,
     0, 0),

    -- Beat 6: Second binding — steeper cost
    (4, 6, 2575, 0, 'witness',
     'quest_complete', 1,
     'Half of what you carry. And a larger piece of your vitality. A fair exchange.',
     'The clock is ticking.',
     0, 0,
     'ileana_ep2_reward', 'from_portal',
     0, 0, 0.0,
     'menacing', 'satisfied',
     'Take it.',
     'The world dims for a moment. Then clarity returns, cold and sharp.',
     0, 0, 0,
     50, 40),

-- ── Episode 3: The Reckoning ─────────────────────────────────────────────────

    -- Beat 7: Final encounter — Ileana comes to collect fully
    (4, 7, 2575, 0, 'witness',
     'encounter_count', 1,
     'You look tired. I wonder why.',
     'There is nowhere left to go.',
     0, 0,
     '', 'from_portal',
     0, 0, 0.0,
     'contemptuous', '',
     'Say what you came to say.',
     '',
     0, 0, 0,
     0, 0),

    -- Beat 8: She lays out the final terms
    (4, 8, 2575, 0, 'witness',
     'encounter_count', 1,
     'The final payment. Everything you have left. Then we are even — and you are free.',
     'Freedom has a price.',
     0, 0,
     '', 'from_portal',
     0, 0, 0.0,
     'contemptuous', '',
     'And if I refuse?',
     '',
     0, 0, 0,
     0, 0),

    -- Beat 9: The final binding — nearly everything is taken
    (4, 9, 2575, 0, 'witness',
     'quest_complete', 1,
     'You always had a choice. You just kept choosing me.',
     'This is not over.',
     0, 0,
     'ileana_ep3_reward', 'from_portal',
     0, 0, 0.0,
     'contemptuous', 'cold',
     'Then take it. We''re done.',
     'She vanishes. The air smells of ash and old copper.',
     0, 0, 0,
     80, 60);
