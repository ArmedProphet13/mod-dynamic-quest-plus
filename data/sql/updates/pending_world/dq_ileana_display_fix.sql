-- DynamicQuests+ | Fix Ileana display ID
-- 22860 is NPC entry "Illidari Succubus", not a display ID.
-- 20387 is the correct CreatureDisplayInfo ID for that model.
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20387 WHERE `CreatureID` = 900020;
