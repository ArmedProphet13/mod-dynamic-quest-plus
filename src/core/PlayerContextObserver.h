/*
 * DynamicQuests+ Module
 * System 1: Player Context Observer
 *
 * Lightweight snapshot of player state, updated every tick.
 * All other systems read from PlayerContext — none query Player directly.
 */

#ifndef DQ_PLAYER_CONTEXT_OBSERVER_H
#define DQ_PLAYER_CONTEXT_OBSERVER_H

#include "Define.h"
#include "Position.h"
#include <cstdint>

enum DQActivityContext : uint8
{
    DQ_ACTIVITY_IDLE      = 0,
    DQ_ACTIVITY_TRAVELING = 1, // distance > 200y in 5min, kills < 2
    DQ_ACTIVITY_GRINDING  = 2, // kills > 5 in 5min
    DQ_ACTIVITY_EXPLORING = 3, // moving, new zone, few kills
};

enum DQItemCategory : uint8
{
    DQ_ITEM_FOOD        = 0,
    DQ_ITEM_DRINK       = 1,
    DQ_ITEM_CLOTH       = 2,
    DQ_ITEM_ORE         = 3,
    DQ_ITEM_HERB        = 4,
    DQ_ITEM_CATEGORY_MAX = 5,
};

struct PlayerContext
{
    uint32 zoneId       = 0;
    uint32 areaId       = 0;
    uint32 mapId        = 0;
    uint8  level        = 0;
    uint8  faction      = 0;
    uint8  race         = 0;

    bool   isOutdoors    = false; // player->IsOutdoors()
    bool   isInCombat    = false;
    bool   isAFK         = false;
    bool   isInDungeon   = false; // dungeon/raid/bg/arena
    bool   isFlying      = false; // flying mount airborne (MOVEMENTFLAG_FLYING)
    bool   isSwimming    = false; // player->IsInWater()

    uint32 goldCopper    = 0;

    bool   hasMovedRecently     = false;
    uint32 lastMoveTimeMs       = 0;
    Position lastPosition       = {};

    DQActivityContext activityContext = DQ_ACTIVITY_IDLE;

    uint32 recentKills5min      = 0;
    float  distanceTraveled5min = 0.f;
    uint32 timeInZoneMinutes    = 0;
    uint32 zoneEntryTimeMs      = 0;

    bool   inventoryHas[DQ_ITEM_CATEGORY_MAX] = {};

    uint32 activityWindowResetTimeMs = 0;
    Position activityWindowStartPos  = {};
};

class Player;

class PlayerContextObserver
{
public:
    static void Update(Player* player, PlayerContext& ctx, uint32 p_time);
    static void OnZoneChanged(Player* player, PlayerContext& ctx, uint32 newZone, uint32 newArea);
    static void OnKill(PlayerContext& ctx);
    static void OnLoot(Player* player, PlayerContext& ctx, uint32 itemId);

private:
    static void DeriveActivityContext(PlayerContext& ctx);
    static void CheckInventoryCategories(Player* player, PlayerContext& ctx);
};

#endif // DQ_PLAYER_CONTEXT_OBSERVER_H
