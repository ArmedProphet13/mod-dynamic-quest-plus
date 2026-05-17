-- DynamicQuests+ Module
-- Courier NPC template table + dedicated creature_template entries.
--
-- Dedicated entries (900001-900005 couriers, 900011-900013 destinations) are
-- required so that ScriptName = 'DQ_CourierAI' / 'DQ_DestinationAI' is set
-- exclusively on these NPCs. Updating ScriptName on existing world NPCs would
-- break their in-world behaviour. Display IDs are sourced from verified entries
-- in creature_template_model and stored in the same table.
--
-- faction 35 = friendly to all (couriers must not attack on sight).
-- ============================================================

-- Courier template mapping table
DROP TABLE IF EXISTS `dq_courier_template`;
CREATE TABLE `dq_courier_template` (
    `entry`       INT          NOT NULL,
    `theme_tag`   VARCHAR(32)  NOT NULL,
    `description` VARCHAR(128) NOT NULL DEFAULT '',
    PRIMARY KEY (`entry`),
    KEY `idx_theme` (`theme_tag`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Dedicated courier creature_template entries.
-- No modelid column in this AzerothCore version — display IDs go in creature_template_model.
INSERT IGNORE INTO `creature_template`
    (`entry`, `name`, `subname`, `minlevel`, `maxlevel`,
     `faction`, `BaseAttackTime`, `RangeAttackTime`, `unit_class`,
     `type`, `type_flags`, `npcflag`, `ScriptName`)
VALUES
-- Demon-themed courier (succubus appearance)
(900001, 'Shadowed Envoy',     'Courier', 1, 1, 35, 2000, 2000, 1, 3, 0, 1, 'DQ_CourierAI'),
-- Child courier (city / charity interactions)
(900002, 'Wandering Orphan',   'Courier', 1, 1, 35, 2000, 2000, 1, 7, 0, 1, 'DQ_CourierAI'),
-- Undead courier (dark / Forsaken interactions)
(900003, 'Restless Spirit',    'Courier', 1, 1, 35, 2000, 2000, 1, 7, 0, 1, 'DQ_CourierAI'),
-- Generic humanoid courier (social / default fallback)
(900004, 'Traveling Stranger', 'Courier', 1, 1, 35, 2000, 2000, 1, 7, 0, 1, 'DQ_CourierAI'),
-- Beast / nature courier
(900005, 'Dire Messenger',     'Courier', 1, 1, 35, 2000, 2000, 1, 1, 0, 1, 'DQ_CourierAI'),
-- Destination NPCs (used by MechanicFetchItem)
(900011, 'Waiting Trader',     'Destination', 1, 1, 35, 2000, 2000, 1, 7, 0, 1, 'DQ_DestinationAI'),
(900012, 'Waiting Child',      'Destination', 1, 1, 35, 2000, 2000, 1, 7, 0, 1, 'DQ_DestinationAI'),
(900013, 'Waiting Guard',      'Destination', 1, 1, 35, 2000, 2000, 1, 7, 0, 1, 'DQ_DestinationAI');

-- Display IDs for dedicated entries (creature_template_model replaces old modelid column).
-- IDs sourced from verified creature_template_model rows in the base DB.
INSERT IGNORE INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES
(900001, 0, 2575,  1.0, 1.0),  -- Succubus / demoness appearance
(900002, 0,  221,  1.0, 1.0),  -- Human boy (child)
(900003, 0, 4041,  1.0, 1.0),  -- Forsaken humanoid (undead)
(900004, 0,   87,  1.0, 1.0),  -- Peasant (generic human)
(900005, 0,  179,  1.0, 1.0),  -- Kodo Beast (nature)
(900011, 0,   87,  1.0, 1.0),  -- Peasant (trader destination)
(900012, 0,  221,  1.0, 1.0),  -- Boy (child destination)
(900013, 0, 3167,  1.0, 1.0);  -- Stormwind City Guard (guard destination)

-- Force npcflag=1 (GOSSIP) and correct ScriptName in case INSERT IGNORE skipped these rows
UPDATE `creature_template`
SET `npcflag` = 1, `ScriptName` = 'DQ_CourierAI'
WHERE `entry` IN (900001, 900002, 900003, 900004, 900005);

-- Courier theme mappings
INSERT INTO `dq_courier_template` (`entry`, `theme_tag`, `description`) VALUES
(900001, 'demon',   'Demonic courier for dark/deal interactions'),
(900002, 'child',   'Child courier for social/charity interactions'),
(900003, 'undead',  'Undead courier for undead/dark interactions'),
(900004, 'social',  'Generic humanoid courier — default fallback'),
(900005, 'nature',  'Beast/nature courier for wilderness interactions');
