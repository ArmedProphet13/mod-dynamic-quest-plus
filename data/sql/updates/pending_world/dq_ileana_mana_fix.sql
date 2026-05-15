-- DynamicQuests+ | Ileana mana fix
-- unit_class=1 (warrior) has no mana pool — creature CastSpell fails power check.
-- unit_class=2 (paladin/caster) gives Ileana a mana pool so Drain Soul fires correctly.
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 900020;
