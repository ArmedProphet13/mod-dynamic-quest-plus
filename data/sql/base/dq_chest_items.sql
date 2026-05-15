-- DynamicQuests+ | Traveler's Treasure Chest items
--
-- Six soulbound loot chests delivered on event completion.
-- Right-click opens a loot window (ITEM_FLAG_HAS_LOOT = 4, same mechanism as
-- Blizzard's Satchel of Helpful Goods / Protectorate Treasure Cache).
-- The item inside is selected dynamically by DQChestOpener (PlayerScript),
-- class-appropriate and level-appropriate. No static loot table needed.
--
-- Entry layout:
--   900100 / 900101 / 900102  Armor Cache  (Quality 2/3/4 = green/blue/purple)
--   900103 / 900104 / 900105  Weapon Cache (Quality 2/3/4 = green/blue/purple)
--
-- displayid 12644 = "Cache of the Shattered Sun" appearance (confirmed working item display in
-- 3.3.5a — item 34548). Quality border (green/blue/purple outline) rendered automatically by
-- the client from the Quality field.
-- Flags = 4 (ITEM_FLAG_HAS_LOOT) enables right-click "Open" without any spell.

DELETE FROM `item_template` WHERE `entry` IN (900100, 900101, 900102, 900103, 900104, 900105);

INSERT INTO `item_template`
    (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`,
     `Quality`, `Flags`, `AllowableClass`, `AllowableRace`, `stackable`,
     `bonding`, `description`,
     `ScriptName`)
VALUES
    -- ── Armor Cache (green) ─────────────────────────────────────────────────
    (900100, 15, 0, -1, 'Traveler''s Armor Cache', 12644,
     2, 4, -1, -1, 1,
     1, 'Open to receive a piece of armor suited to your class and level. Bound to the finder.',
     ''),

    -- ── Armor Cache (blue) ──────────────────────────────────────────────────
    (900101, 15, 0, -1, 'Traveler''s Armor Cache', 12644,
     3, 4, -1, -1, 1,
     1, 'Open to receive quality armor suited to your class and level. Bound to the finder.',
     ''),

    -- ── Armor Cache (purple) ────────────────────────────────────────────────
    (900102, 15, 0, -1, 'Traveler''s Armor Cache', 12644,
     4, 4, -1, -1, 1,
     1, 'Open to receive exceptional armor worthy of a champion. Bound to the finder.',
     ''),

    -- ── Weapon Cache (green) ────────────────────────────────────────────────
    (900103, 15, 0, -1, 'Traveler''s Weapon Cache', 12644,
     2, 4, -1, -1, 1,
     1, 'Open to receive a weapon suited to your class and level. Bound to the finder.',
     ''),

    -- ── Weapon Cache (blue) ─────────────────────────────────────────────────
    (900104, 15, 0, -1, 'Traveler''s Weapon Cache', 12644,
     3, 4, -1, -1, 1,
     1, 'Open to receive a quality weapon suited to your class and level. Bound to the finder.',
     ''),

    -- ── Weapon Cache (purple) ───────────────────────────────────────────────
    (900105, 15, 0, -1, 'Traveler''s Weapon Cache', 12644,
     4, 4, -1, -1, 1,
     1, 'Open to receive an exceptional weapon worthy of a champion. Bound to the finder.',
     '');
