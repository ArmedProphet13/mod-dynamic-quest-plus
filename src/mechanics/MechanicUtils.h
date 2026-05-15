/*
 * DynamicQuests+ Module
 * Shared mechanic utilities: reward delivery + outcome_data parsing.
 */

#ifndef DQ_MECHANIC_UTILS_H
#define DQ_MECHANIC_UTILS_H

#include "Define.h"
#include <string>

class Player;
class Item;

namespace DQMechanicUtils
{
    // Deliver a random reward from dq_reward_pool for pool and level.
    void DeliverReward(Player* player, const std::string& poolName, uint8 playerLevel);

    // ── Inventory scanning ───────────────────────────────────────────────────
    // These cover BOTH the main backpack (slots 23-38) and any extra bags (slots 19-22).
    // Use these instead of rolling your own loop — the bag/slot constants are tricky.

    // First item with matching class + subclass, or nullptr.
    Item* FindFirstItemByClass(Player* player, uint32 cls, uint32 subCls);

    // First item with matching entry ID, or nullptr.
    Item* FindFirstItemByEntry(Player* player, uint32 itemEntry);

    // ── Outcome-data parsing ─────────────────────────────────────────────────
    // Parse "key:value" from comma-separated outcome_data string.
    // Example: ParseUint32("count:5,entry:1234", "count") → 5
    uint32      ParseUint32(const std::string& data, const std::string& key, uint32 defaultVal = 0);
    float       ParseFloat (const std::string& data, const std::string& key, float  defaultVal = 0.f);
    std::string ParseString(const std::string& data, const std::string& key, const std::string& defaultVal = {});
}

#endif // DQ_MECHANIC_UTILS_H
