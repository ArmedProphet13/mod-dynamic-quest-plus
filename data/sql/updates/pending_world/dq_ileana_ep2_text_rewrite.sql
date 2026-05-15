-- DynamicQuests+ | Ileana Episode II — text rewrite
-- Collapses 4 pages down to 2. Old pages 9001022/9001023 removed.

DELETE FROM `npc_text` WHERE `ID` IN (9001020, 9001021, 9001022, 9001023);

-- Page 1: She's back. The real pitch.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001020,
'$B$BThe portal opens. Same violet fire. Same silence before it, like the air knew it was coming and decided not to warn you.$B$B"You thought about it. I know you did. That''s why I''m back."$B$BShe tilts her head.$B$B"You''ve been watching others rise. People with less than you — less discipline, less sacrifice — and somehow they keep getting further ahead. That bothers you. It should."$B$B"What I''m offering doesn''t cost you anything you''re using. Think of it less as a deal and more as correcting an imbalance. You''ve already paid more than most. I''m just here to make sure you get something back."$B$B');

-- Page 2 (final): The close.
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
(9001021,
'$B$B"Nothing you''ll miss!"$B$B"You won''t even know what you gave. That''s the beauty of it."$B$B"Power without the weight of wondering whether you deserve it. Doesn''t that sound like relief?"$B$B');
