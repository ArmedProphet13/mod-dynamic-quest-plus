-- DynamicQuests+ | Ileana faction fix
-- faction 35 (friendly to all) blocks CastSpell on player with hostile spells.
-- faction 14 (hostile to all) allows non-triggered hostile spell casts.
-- SetFaction(14) is called at runtime for combat; this sets the DB default.
UPDATE `creature_template` SET `faction` = 14 WHERE `entry` = 900020;
