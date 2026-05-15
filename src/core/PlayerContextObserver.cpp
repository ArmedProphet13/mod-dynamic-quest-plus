/*
 * DynamicQuests+ Module
 * System 1: Player Context Observer — Implementation
 */

#include "PlayerContextObserver.h"
#include "Config.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Map.h"
#include "Player.h"

void PlayerContextObserver::Update(Player* player, PlayerContext& ctx, uint32 /*p_time*/)
{
    ctx.mapId      = player->GetMapId();
    ctx.level      = static_cast<uint8>(player->GetLevel());
    ctx.faction    = static_cast<uint8>(player->GetTeamId());
    ctx.race       = static_cast<uint8>(player->getRace());
    ctx.isInCombat = player->IsInCombat();
    ctx.isAFK      = player->isAFK();
    ctx.goldCopper = player->GetMoney();

    Map* map     = player->GetMap();
    ctx.isInDungeon = map->IsDungeon() || map->IsRaid() || player->InBattleground();
    ctx.isOutdoors  = player->IsOutdoors();
    ctx.isFlying    = player->IsFlying();
    ctx.isSwimming  = player->IsInWater();

    uint32 nowMs = GameTime::GetGameTimeMS().count();

    Position currentPos;
    currentPos.Relocate(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    if (currentPos.GetExactDist(&ctx.lastPosition) > 0.5f)
    {
        ctx.lastMoveTimeMs = nowMs;
        ctx.lastPosition   = currentPos;
    }

    uint32 windowMs = sConfigMgr->GetOption<uint32>("DynamicQuests.ActivityWindowSeconds", 30) * 1000;
    ctx.hasMovedRecently = (nowMs - ctx.lastMoveTimeMs) < windowMs;

    if (ctx.activityWindowResetTimeMs == 0)
        ctx.activityWindowResetTimeMs = nowMs;

    if (nowMs - ctx.activityWindowResetTimeMs >= 300000u)
    {
        ctx.distanceTraveled5min     = 0.f;
        ctx.recentKills5min          = 0;
        ctx.activityWindowResetTimeMs = nowMs;
    }
    else
    {
        ctx.distanceTraveled5min += currentPos.GetExactDist(&ctx.lastPosition);
    }

    if (ctx.zoneEntryTimeMs == 0)
        ctx.zoneEntryTimeMs = nowMs;
    ctx.timeInZoneMinutes = (nowMs - ctx.zoneEntryTimeMs) / 60000u;

    DeriveActivityContext(ctx);
}

void PlayerContextObserver::OnZoneChanged(Player* /*player*/, PlayerContext& ctx,
    uint32 newZone, uint32 newArea)
{
    ctx.zoneId            = newZone;
    ctx.areaId            = newArea;
    ctx.zoneEntryTimeMs   = GameTime::GetGameTimeMS().count();
    ctx.timeInZoneMinutes = 0;
    ctx.recentKills5min   = 0;
    ctx.distanceTraveled5min = 0.f;
}

void PlayerContextObserver::OnKill(PlayerContext& ctx)
{
    ++ctx.recentKills5min;
}

void PlayerContextObserver::OnLoot(Player* player, PlayerContext& ctx, uint32 /*itemId*/)
{
    CheckInventoryCategories(player, ctx);
}

void PlayerContextObserver::DeriveActivityContext(PlayerContext& ctx)
{
    if (ctx.recentKills5min >= 5)
        { ctx.activityContext = DQ_ACTIVITY_GRINDING; return; }
    if (ctx.distanceTraveled5min >= 200.f && ctx.recentKills5min < 2)
        { ctx.activityContext = DQ_ACTIVITY_TRAVELING; return; }
    if (ctx.distanceTraveled5min >= 50.f && ctx.recentKills5min < 2 && ctx.timeInZoneMinutes < 10)
        { ctx.activityContext = DQ_ACTIVITY_EXPLORING; return; }
    ctx.activityContext = DQ_ACTIVITY_IDLE;
}

void PlayerContextObserver::CheckInventoryCategories(Player* player, PlayerContext& ctx)
{
    for (uint8 i = 0; i < DQ_ITEM_CATEGORY_MAX; ++i)
        ctx.inventoryHas[i] = false;

    for (uint8 bag = INVENTORY_SLOT_BAG_0; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        for (uint8 slot = 0; slot < MAX_BAG_SIZE; ++slot)
        {
            Item* item = player->GetItemByPos(bag, slot);
            if (!item)
                continue;
            const ItemTemplate* proto = item->GetTemplate();
            if (!proto)
                continue;

            if (proto->Class == ITEM_CLASS_CONSUMABLE)
            {
                if (proto->SubClass == ITEM_SUBCLASS_FOOD)
                {
                    ctx.inventoryHas[DQ_ITEM_FOOD]  = true;
                    ctx.inventoryHas[DQ_ITEM_DRINK] = true; // food and drink share subclass in WotLK
                }
            }
            if (proto->Class == ITEM_CLASS_TRADE_GOODS)
            {
                if (proto->SubClass == ITEM_SUBCLASS_CLOTH)      ctx.inventoryHas[DQ_ITEM_CLOTH] = true;
                if (proto->SubClass == ITEM_SUBCLASS_METAL_STONE) ctx.inventoryHas[DQ_ITEM_ORE]  = true;
                if (proto->SubClass == ITEM_SUBCLASS_HERB)       ctx.inventoryHas[DQ_ITEM_HERB]  = true;
            }
        }
    }
}
