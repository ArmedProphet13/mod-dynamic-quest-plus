-- DynamicQuests+ | Bootstrap Script
-- Run ONCE (or re-run to regenerate) to build the world NPC catalogue.
-- Reads creature_template and creature (spawns) to classify all viable interaction actors.
-- Safe to re-run: truncates and rebuilds dq_world_npc_catalogue.
--
-- Usage: SOURCE this file, then call:
--   CALL dq_bootstrap_catalogue();

DELIMITER $$

DROP PROCEDURE IF EXISTS dq_bootstrap_catalogue$$
CREATE PROCEDURE dq_bootstrap_catalogue()
BEGIN
    DECLARE done INT DEFAULT FALSE;
    DECLARE v_entry INT UNSIGNED;
    DECLARE v_name VARCHAR(100);
    DECLARE v_subname VARCHAR(100);
    DECLARE v_type TINYINT;
    DECLARE v_rank TINYINT;
    DECLARE v_faction SMALLINT;
    DECLARE v_npcflag INT;
    DECLARE v_has_model TINYINT;
    DECLARE v_zone INT;
    DECLARE v_tags VARCHAR(255);
    DECLARE v_threat TINYINT;
    DECLARE v_excluded TINYINT;

    -- Cursor: one row per creature_template entry.
    -- Zone is fetched via subquery (MIN) to avoid the multi-spawn JOIN fanout
    -- that caused duplicate primary key violations on dq_world_npc_catalogue.
    DECLARE cur CURSOR FOR
        SELECT
            ct.entry,
            ct.name,
            ct.subname,
            ct.type,
            ct.rank,
            ct.faction,
            ct.npcflag,
            1 AS has_model,
            COALESCE((SELECT MIN(c.zoneId) FROM creature c WHERE c.id1 = ct.entry), 0) AS zone_id
        FROM creature_template ct
        INNER JOIN creature_template_model ctm ON ctm.CreatureID = ct.entry AND ctm.Idx = 0
        WHERE ct.entry NOT IN (SELECT entry FROM dq_npc_blacklist)
          AND ct.name NOT LIKE '%trigger%'
          AND ct.name NOT LIKE '%Trigger%'
          AND ct.name NOT LIKE '%invisible%'
        ORDER BY ct.entry;

    DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;

    TRUNCATE TABLE dq_world_npc_catalogue;

    OPEN cur;

    read_loop: LOOP
        FETCH cur INTO v_entry, v_name, v_subname, v_type, v_rank,
                       v_faction, v_npcflag, v_has_model, v_zone;
        IF done THEN
            LEAVE read_loop;
        END IF;

        -- Default values
        SET v_tags = '';
        SET v_threat = 1;
        SET v_excluded = 0;

        -- === EXCLUSION RULES ===
        -- Exclude bosses, elites, rares
        IF v_rank IN (2, 3, 4) THEN  -- ELITE=1 is borderline; BOSS=3, RARE_ELITE=4
            SET v_excluded = 1;
        END IF;

        -- Exclude vehicles (type 7)
        IF v_type = 7 THEN
            SET v_excluded = 1;
        END IF;

        -- Exclude critters and wild animals that make no sense as couriers (type 8=critter)
        IF v_type = 8 THEN
            SET v_excluded = 1;
        END IF;

        -- === THREAT LEVEL ===
        -- Without mindmg, use npcflag as a proxy: NPCs with interaction flags
        -- (gossip, vendor, quest) are civilian/non-combat at rank 0.
        IF v_rank = 0 AND v_npcflag > 0 THEN
            SET v_threat = 0;  -- safe civilian (has NPC function, not a pure fighter)
        ELSEIF v_rank = 0 THEN
            SET v_threat = 1;  -- standard mob
        ELSEIF v_rank = 1 THEN
            SET v_threat = 2;  -- elite
        ELSE
            SET v_threat = 3;  -- boss/rare elite
        END IF;

        -- === TAG DERIVATION ===

        -- Creature type tags
        IF v_type = 1 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',',' ), 'beast'); END IF;
        IF v_type = 2 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'dragonkin'); END IF;
        IF v_type = 3 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'demon'); END IF;
        IF v_type = 4 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'elemental'); END IF;
        IF v_type = 5 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'giant'); END IF;
        IF v_type = 6 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'undead'); END IF;
        IF v_type = 7 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'humanoid'); END IF;
        IF v_type = 10 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'mechanical'); END IF;
        IF v_type = 11 THEN SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'elemental'); END IF;

        -- Humanoid type (type=7 in creature_type enum)
        IF v_type = 7 THEN
            -- Humanoid demons
            IF v_tags LIKE '%demon%' OR v_faction IN (14, 90, 154, 1610, 1611) THEN
                SET v_tags = CONCAT(v_tags, ',humanoid_demon');
            END IF;
        END IF;

        -- Faction alliance/horde
        -- Alliance factions: roughly < 500 and friendly
        IF v_faction IN (1, 2, 3, 4, 5, 12, 57, 85, 890, 891, 892, 893, 894, 1037, 1052) THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'alliance');
        END IF;
        -- Horde factions
        IF v_faction IN (29, 76, 530, 531, 1214, 1374, 1375, 1376, 1604, 1629) THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'horde');
        END IF;
        -- Hostile to all
        IF v_faction = 14 THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'hostile');
        END IF;
        -- Neutral/friendly
        IF v_faction IN (35, 68, 72, 189) THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'neutral');
        END IF;

        -- Child/orphan detection by name
        IF v_name LIKE '%Child%' OR v_name LIKE '%Orphan%' OR v_name LIKE '%Kid%'
            OR v_subname LIKE '%child%' OR v_name LIKE '%Boy%' OR v_name LIKE '%Girl%'
        THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'child');
        END IF;

        -- Civilian / non-combat (npcflag > 0 = has interaction function, not a pure fighter)
        IF v_npcflag > 0 AND v_rank = 0 THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'civilian');
        END IF;

        -- Merchant/vendor
        IF v_npcflag & 128 THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'merchant');
        END IF;

        -- Guard/soldier
        IF v_subname LIKE '%Guard%' OR v_subname LIKE '%Soldier%' OR v_subname LIKE '%Sentry%'
            OR v_name LIKE '%Guard%'
        THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'guard');
        END IF;

        -- Humanoid fallback (type 7 was already tagged, add generic humanoid to others that look human)
        IF v_type = 7 AND v_tags NOT LIKE '%humanoid%' THEN
            SET v_tags = CONCAT(v_tags, IF(v_tags='','',','), 'humanoid');
        END IF;

        -- Insert into catalogue
        INSERT INTO dq_world_npc_catalogue
            (entry, zone_id, creature_type, faction, tags, threat_level, excluded)
        VALUES
            (v_entry, v_zone, v_type, v_faction, v_tags, v_threat, v_excluded);

    END LOOP;

    CLOSE cur;

    SELECT CONCAT('dq_bootstrap_catalogue complete. Entries: ', COUNT(*)) AS result
    FROM dq_world_npc_catalogue;
END$$

DELIMITER ;

-- ---------------------------------------------------------------------------
-- dq_bootstrap_tier1_quests
-- Stub procedure — tier-1 content is authored via data/sql/interactions/*.sql
-- files, not auto-generated. This procedure exists so .dq bootstrap does not
-- error when calling CALL dq_bootstrap_tier1_quests().
-- ---------------------------------------------------------------------------
DELIMITER $$

DROP PROCEDURE IF EXISTS dq_bootstrap_tier1_quests$$
CREATE PROCEDURE dq_bootstrap_tier1_quests()
BEGIN
    SELECT CONCAT('dq_bootstrap_tier1_quests: ',
        COUNT(*), ' interaction template(s) currently loaded.') AS result
    FROM dq_interaction_template;
END$$

DELIMITER ;
