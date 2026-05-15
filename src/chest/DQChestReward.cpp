/*
 * DynamicQuests+ Module — Traveler's Treasure Chest
 *
 * Items 900100-900102: Traveler's Armor Cache  (green/blue/purple)
 * Items 900103-900105: Traveler's Weapon Cache (green/blue/purple)
 *
 * Uses Blizzard's ITEM_FLAG_HAS_LOOT mechanism: right-clicking the chest
 * opens an authentic loot window. The item inside is chosen in-memory —
 * class-appropriate, level-appropriate — with no DB query.
 *
 * Hook: PlayerScript::OnPlayerBeforeOpenItem fires before SendLoot.
 * We pre-populate item->loot and set m_lootGenerated = true so the engine's
 * FillLoot (which reads item_loot_template) is bypassed entirely.
 * After the player takes the item, the engine auto-destroys the chest.
 */

#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include <cctype>
#include <string>

// ── Entry layout ──────────────────────────────────────────────────────────────
// 900100 / 900103  Green  (Quality 2, Uncommon)
// 900101 / 900104  Blue   (Quality 3, Rare)
// 900102 / 900105  Purple (Quality 4, Epic)

static constexpr uint32 DQ_CHEST_BASE = 900100;
static constexpr uint32 DQ_CHEST_MAX  = 900105;

static bool   IsDQChest(uint32 e)     { return e >= DQ_CHEST_BASE && e <= DQ_CHEST_MAX; }
static uint8  ChestTier(uint32 e)     { return static_cast<uint8>((e - DQ_CHEST_BASE) % 3); }
static bool   ChestIsWeapon(uint32 e) { return (e - DQ_CHEST_BASE) >= 3; }
static uint32 TierToQuality(uint8 t)  { return static_cast<uint32>(t) + 2; }

// ── Dev/test item filter ──────────────────────────────────────────────────────
// Reject items whose names contain dev-only patterns (case-insensitive).

static bool IsDevOrTestItem(std::string const& name)
{
    std::string low = name;
    for (char& c : low)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (low.find("test")        != std::string::npos) return true;
    if (low.find("deprecated")  != std::string::npos) return true;
    if (low.find("placeholder") != std::string::npos) return true;
    if (low.find("do not use")  != std::string::npos) return true;
    if (low.find("unused")      != std::string::npos) return true;
    if (low.find("old ")        == 0)                 return true; // "Old Foo" prefix
    if (!low.empty() && low[0] == '[')                return true; // "[PH] ..." placeholders
    return false;
}

// ── Raid-tier item level filter ───────────────────────────────────────────────
// Items from raids have inflated ItemLevel relative to their RequiredLevel.
// Thresholds are tuned per expansion tier to block raid drops while keeping
// world/quest/dungeon gear. Items with ItemLevel 0 are skipped (containers, etc.)

static bool IsTooHighItemLevel(uint32 itemLevel, uint32 reqLevel)
{
    if (itemLevel == 0) return false;
    uint32 maxIlvl;
    if      (reqLevel <= 60) maxIlvl = reqLevel + 13;   // vanilla: excludes most MC/BWL
    else if (reqLevel <= 70) maxIlvl = reqLevel + 35;   // BC: excludes T4+ raid drops
    else                     maxIlvl = reqLevel + 115;  // WotLK: excludes Naxx-25+
    return itemLevel > maxIlvl;
}

// ── Slot gates ────────────────────────────────────────────────────────────────
// Armor chest: body pieces only. No rings, necks, cloaks, trinkets, relics.

static bool IsArmorBodySlot(uint32 inv)
{
    switch (inv)
    {
        case INVTYPE_HEAD:
        case INVTYPE_SHOULDERS:
        case INVTYPE_CHEST:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
        case INVTYPE_ROBE:     // visually identical to chest slot
            return true;
        default:
            return false;
    }
}

static bool IsWeaponSlot(uint32 inv)
{
    switch (inv)
    {
        case INVTYPE_WEAPON:
        case INVTYPE_RANGED:
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:
            return true;
        default:
            return false;
    }
}

// ── Proficiency gates ─────────────────────────────────────────────────────────

static uint32 MaxArmorSubclass(uint32 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        default: // Priest, Mage, Warlock
            return ITEM_SUBCLASS_ARMOR_CLOTH;
    }
}

static bool WeaponProficient(uint32 cls, uint32 sub)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
            return sub == ITEM_SUBCLASS_WEAPON_AXE    || sub == ITEM_SUBCLASS_WEAPON_AXE2   ||
                   sub == ITEM_SUBCLASS_WEAPON_MACE   || sub == ITEM_SUBCLASS_WEAPON_MACE2  ||
                   sub == ITEM_SUBCLASS_WEAPON_SWORD  || sub == ITEM_SUBCLASS_WEAPON_SWORD2 ||
                   sub == ITEM_SUBCLASS_WEAPON_POLEARM|| sub == ITEM_SUBCLASS_WEAPON_STAFF  ||
                   sub == ITEM_SUBCLASS_WEAPON_FIST   || sub == ITEM_SUBCLASS_WEAPON_DAGGER;

        case CLASS_PALADIN:
            return sub == ITEM_SUBCLASS_WEAPON_SWORD  || sub == ITEM_SUBCLASS_WEAPON_SWORD2 ||
                   sub == ITEM_SUBCLASS_WEAPON_MACE   || sub == ITEM_SUBCLASS_WEAPON_MACE2  ||
                   sub == ITEM_SUBCLASS_WEAPON_AXE    || sub == ITEM_SUBCLASS_WEAPON_AXE2   ||
                   sub == ITEM_SUBCLASS_WEAPON_POLEARM;

        case CLASS_HUNTER:
            return sub == ITEM_SUBCLASS_WEAPON_AXE    || sub == ITEM_SUBCLASS_WEAPON_AXE2   ||
                   sub == ITEM_SUBCLASS_WEAPON_SWORD  || sub == ITEM_SUBCLASS_WEAPON_SWORD2 ||
                   sub == ITEM_SUBCLASS_WEAPON_DAGGER || sub == ITEM_SUBCLASS_WEAPON_STAFF  ||
                   sub == ITEM_SUBCLASS_WEAPON_POLEARM|| sub == ITEM_SUBCLASS_WEAPON_FIST   ||
                   sub == ITEM_SUBCLASS_WEAPON_BOW    || sub == ITEM_SUBCLASS_WEAPON_GUN    ||
                   sub == ITEM_SUBCLASS_WEAPON_CROSSBOW|| sub == ITEM_SUBCLASS_WEAPON_THROWN;

        case CLASS_ROGUE:
            return sub == ITEM_SUBCLASS_WEAPON_SWORD  || sub == ITEM_SUBCLASS_WEAPON_DAGGER ||
                   sub == ITEM_SUBCLASS_WEAPON_FIST   || sub == ITEM_SUBCLASS_WEAPON_MACE   ||
                   sub == ITEM_SUBCLASS_WEAPON_AXE    || sub == ITEM_SUBCLASS_WEAPON_BOW    ||
                   sub == ITEM_SUBCLASS_WEAPON_GUN    || sub == ITEM_SUBCLASS_WEAPON_CROSSBOW||
                   sub == ITEM_SUBCLASS_WEAPON_THROWN;

        case CLASS_PRIEST:
            return sub == ITEM_SUBCLASS_WEAPON_STAFF  || sub == ITEM_SUBCLASS_WEAPON_DAGGER ||
                   sub == ITEM_SUBCLASS_WEAPON_MACE   || sub == ITEM_SUBCLASS_WEAPON_WAND;

        case CLASS_SHAMAN:
            return sub == ITEM_SUBCLASS_WEAPON_AXE    || sub == ITEM_SUBCLASS_WEAPON_AXE2   ||
                   sub == ITEM_SUBCLASS_WEAPON_MACE   || sub == ITEM_SUBCLASS_WEAPON_MACE2  ||
                   sub == ITEM_SUBCLASS_WEAPON_DAGGER || sub == ITEM_SUBCLASS_WEAPON_STAFF  ||
                   sub == ITEM_SUBCLASS_WEAPON_FIST;

        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return sub == ITEM_SUBCLASS_WEAPON_STAFF  || sub == ITEM_SUBCLASS_WEAPON_DAGGER ||
                   sub == ITEM_SUBCLASS_WEAPON_SWORD  || sub == ITEM_SUBCLASS_WEAPON_WAND;

        case CLASS_DRUID:
            return sub == ITEM_SUBCLASS_WEAPON_MACE   || sub == ITEM_SUBCLASS_WEAPON_MACE2  ||
                   sub == ITEM_SUBCLASS_WEAPON_STAFF  || sub == ITEM_SUBCLASS_WEAPON_FIST   ||
                   sub == ITEM_SUBCLASS_WEAPON_DAGGER || sub == ITEM_SUBCLASS_WEAPON_POLEARM;

        case CLASS_DEATH_KNIGHT:
            return sub == ITEM_SUBCLASS_WEAPON_AXE    || sub == ITEM_SUBCLASS_WEAPON_AXE2   ||
                   sub == ITEM_SUBCLASS_WEAPON_MACE   || sub == ITEM_SUBCLASS_WEAPON_MACE2  ||
                   sub == ITEM_SUBCLASS_WEAPON_SWORD  || sub == ITEM_SUBCLASS_WEAPON_SWORD2 ||
                   sub == ITEM_SUBCLASS_WEAPON_POLEARM|| sub == ITEM_SUBCLASS_WEAPON_FIST   ||
                   sub == ITEM_SUBCLASS_WEAPON_DAGGER;

        default:
            return true;
    }
}

// ── Candidate selection ───────────────────────────────────────────────────────

static uint32 FindCandidate(Player* player, uint8 tier, bool wantWeapon)
{
    uint8  pLevel    = player->GetLevel();
    if (pLevel > 79) pLevel = 79;  // level 80 → use 79 bracket (endgame gear exists)

    uint32 quality   = TierToQuality(tier);
    uint32 itemClass = wantWeapon ? ITEM_CLASS_WEAPON : ITEM_CLASS_ARMOR;
    uint32 classMask = player->getClassMask();
    uint32 cls       = player->getClass();

    uint32 levelMin  = (pLevel > 3) ? static_cast<uint32>(pLevel - 3) : 1;
    uint32 levelMax  = static_cast<uint32>(pLevel);

    std::vector<uint32> candidates;
    candidates.reserve(64);

    for (ItemTemplate const* tmpl : *sObjectMgr->GetItemTemplateStoreFast())
    {
        if (!tmpl)                                                         continue;
        if (tmpl->Quality   != quality)                                    continue;
        if (tmpl->Class     != itemClass)                                  continue;
        if (tmpl->RequiredLevel == 0)                                      continue;
        if (tmpl->RequiredLevel < levelMin || tmpl->RequiredLevel > levelMax) continue;
        if (tmpl->Bonding   == BIND_QUEST_ITEM || tmpl->Bonding == BIND_QUEST_ITEM1) continue;
        if (tmpl->StartQuest != 0)                                         continue;
        if (tmpl->SellPrice == 0)                                          continue;
        if (IsDevOrTestItem(tmpl->Name1))                                  continue;
        if (IsTooHighItemLevel(tmpl->ItemLevel, tmpl->RequiredLevel))      continue;

        if (wantWeapon)
        {
            if (!IsWeaponSlot(tmpl->InventoryType))                        continue;
            if (!WeaponProficient(cls, tmpl->SubClass))                    continue;
        }
        else
        {
            if (!IsArmorBodySlot(tmpl->InventoryType))                     continue;
            // Reject armor heavier than the class can wear
            if (tmpl->SubClass >= ITEM_SUBCLASS_ARMOR_CLOTH &&
                tmpl->SubClass <= ITEM_SUBCLASS_ARMOR_PLATE)
            {
                if (tmpl->SubClass > MaxArmorSubclass(cls))                continue;
            }
            if (tmpl->SubClass >= ITEM_SUBCLASS_ARMOR_LIBRAM)             continue; // no relics
        }

        // AllowableClass -1 in DB becomes 0xFFFFFFFF as uint32
        if (tmpl->AllowableClass != 0xFFFFFFFF &&
            !(tmpl->AllowableClass & classMask))                           continue;

        candidates.push_back(tmpl->ItemId);
    }

    if (candidates.empty())
        return 0;

    return candidates[urand(0, static_cast<uint32>(candidates.size()) - 1)];
}

// ── PlayerScript ──────────────────────────────────────────────────────────────

class DQChestOpener : public PlayerScript
{
public:
    DQChestOpener() : PlayerScript("dq_chest_opener", { PLAYERHOOK_ON_BEFORE_OPEN_ITEM }) { }

    bool OnPlayerBeforeOpenItem(Player* player, Item* item) override
    {
        if (!IsDQChest(item->GetEntry()))
            return true;   // not our chest — normal loot flow

        // Don't re-roll if already generated (player closed window without taking the item)
        if (item->m_lootGenerated)
            return true;

        uint32 entry     = item->GetEntry();
        uint8  tier      = ChestTier(entry);
        bool   isWeapon  = ChestIsWeapon(entry);

        uint32 gearEntry = FindCandidate(player, tier, isWeapon);

        // Mark as generated first: prevents FillLoot from overwriting our content
        item->m_lootGenerated = true;
        item->loot.clear();
        item->loot.containerGUID = item->GetGUID();

        if (gearEntry == 0)
        {
            LOG_WARN("module.dynamicquests.chest",
                "DQChest: no candidate found — player={} level={} tier={} weapon={}",
                player->GetName(), player->GetLevel(), tier, isWeapon);
            // unlootedCount stays 0 → isLooted() = true → engine destroys chest on release
            return true;
        }

        LootItem li{};
        li.itemid            = gearEntry;
        li.count             = 1;
        li.itemIndex         = 0;        // first (and only) item in the vector
        li.is_looted         = false;
        li.is_blocked        = false;
        li.freeforall        = false;
        li.is_underthreshold = true;     // won't trigger group loot rolls
        li.is_counted        = true;
        li.needs_quest       = false;
        li.follow_loot_rules = false;

        item->loot.items.push_back(li);
        item->loot.unlootedCount = 1;   // one item to take → engine destroys chest after

        return true;   // proceed to SendLoot → loot window opens with our item
    }
};

void AddSC_dq_chest_items()
{
    new DQChestOpener();
}
