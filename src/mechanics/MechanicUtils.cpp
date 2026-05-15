/*
 * DynamicQuests+ Module
 * Shared mechanic utilities — Implementation
 */

#include "MechanicUtils.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Player.h"
#include "Container/Bag.h"
#include <random>

namespace DQMechanicUtils
{

Item* FindFirstItemByClass(Player* player, uint32 cls, uint32 subCls)
{
    // Backpack: bag=255 (INVENTORY_SLOT_BAG_0), slots 23-38
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item) continue;
        const ItemTemplate* t = item->GetTemplate();
        if (t && t->Class == cls && t->SubClass == subCls)
            return item;
    }
    // Extra bags: bag slots 19-22
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Bag* bag = player->GetBagByPos(bagSlot);
        if (!bag) continue;
        for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
        {
            Item* item = player->GetItemByPos(bagSlot, slot);
            if (!item) continue;
            const ItemTemplate* t = item->GetTemplate();
            if (t && t->Class == cls && t->SubClass == subCls)
                return item;
        }
    }
    return nullptr;
}

Item* FindFirstItemByEntry(Player* player, uint32 itemEntry)
{
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetEntry() == itemEntry)
            return item;
    }
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Bag* bag = player->GetBagByPos(bagSlot);
        if (!bag) continue;
        for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
        {
            Item* item = player->GetItemByPos(bagSlot, slot);
            if (item && item->GetEntry() == itemEntry)
                return item;
        }
    }
    return nullptr;
}

void DeliverReward(Player* player, const std::string& poolName, uint8 playerLevel)
{
    QueryResult result = WorldDatabase.Query(
        "SELECT item_entry, gold_min_copper, gold_max_copper, xp_amount "
        "FROM dq_reward_pool "
        "WHERE pool_name='{}' AND level_min<={} AND level_max>={} "
        "ORDER BY RAND() LIMIT 1",
        poolName, playerLevel, playerLevel);

    if (!result)
    {
        LOG_WARN("module.dynamicquests.mechanic", "DeliverReward: no entry for pool='{}' level={}",
            poolName, playerLevel);
        return;
    }

    Field* f       = result->Fetch();
    uint32 itemId  = f[0].Get<uint32>();
    uint32 goldMin = f[1].Get<uint32>();
    uint32 goldMax = f[2].Get<uint32>();
    uint32 xp      = f[3].Get<uint32>();

    if (itemId != 0)
    {
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1) == EQUIP_ERR_OK)
        {
            Item* item = player->StoreNewItem(dest, itemId, true);
            if (item)
                player->SendNewItem(item, 1, true, false);
        }
    }

    if (goldMax > goldMin)
    {
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<uint32> roll(goldMin, goldMax);
        player->ModifyMoney(static_cast<int32>(roll(rng)));
    }
    else if (goldMin > 0)
    {
        player->ModifyMoney(static_cast<int32>(goldMin));
    }

    if (xp > 0 && !player->IsMaxLevel())
        player->GiveXP(xp, nullptr);
}

uint32 ParseUint32(const std::string& data, const std::string& key, uint32 defaultVal)
{
    std::string needle = key + ":";
    size_t pos = data.find(needle);
    if (pos == std::string::npos)
        return defaultVal;
    try { return static_cast<uint32>(std::stoul(data.substr(pos + needle.size()))); }
    catch (...) { return defaultVal; }
}

float ParseFloat(const std::string& data, const std::string& key, float defaultVal)
{
    std::string needle = key + ":";
    size_t pos = data.find(needle);
    if (pos == std::string::npos)
        return defaultVal;
    try { return std::stof(data.substr(pos + needle.size())); }
    catch (...) { return defaultVal; }
}

std::string ParseString(const std::string& data, const std::string& key, const std::string& defaultVal)
{
    std::string needle = key + ":";
    size_t pos = data.find(needle);
    if (pos == std::string::npos)
        return defaultVal;
    std::string rest = data.substr(pos + needle.size());
    size_t comma = rest.find(',');
    return (comma != std::string::npos) ? rest.substr(0, comma) : rest;
}

} // namespace DQMechanicUtils
