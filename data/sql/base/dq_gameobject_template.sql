-- DQ+ interactable prop GOs (entries 900100-900102)
-- Type 22 = GOOBER: fully scriptable interactive object, no loot window.
-- All data fields 0 = no spell, no lock, no cooldown — script handles everything.
-- Display IDs reuse existing WoW herb/crate/satchel models.
--   900100 Herb Bundle  : displayId 698  (Goldthorn herb model)
--   900101 Supply Crate : displayId 31   (Excavation Supply Crate)
--   900102 Fallen Pack  : displayId 323  (Noggle's Satchel)

DELETE FROM `gameobject_template` WHERE `entry` IN (900100, 900101, 900102);

INSERT INTO `gameobject_template`
    (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`,
     `Data0`, `Data1`, `Data2`, `Data3`, `Data4`, `Data5`, `Data6`, `Data7`,
     `Data8`, `Data9`, `Data10`, `Data11`, `Data12`, `Data13`, `Data14`, `Data15`,
     `Data16`, `Data17`, `Data18`, `Data19`, `Data20`, `Data21`, `Data22`, `Data23`,
     `AIName`, `ScriptName`, `VerifiedBuild`)
VALUES
(900100, 22, 698,  'DQ Herb Bundle',   '', '', '', 1.0,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, '', 'DQ_PropGO', 0),
(900101, 22, 31,   'DQ Supply Crate',  '', '', '', 1.0,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, '', 'DQ_PropGO', 0),
(900102, 22, 323,  'DQ Fallen Pack',   '', '', '', 1.0,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, '', 'DQ_PropGO', 0);
