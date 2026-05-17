/*
 * DynamicQuests+ Module
 * DQSpawnSystem — Implementation
 */

#include "DQSpawnSystem.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "Position.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include <cmath>

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

Position DQSpawnSystem::CalcNearPoint(Player* player, float dist, float worldAngle)
{
    float x = player->GetPositionX();
    float y = player->GetPositionY();
    float z = player->GetPositionZ();
    player->GetNearPoint(nullptr, x, y, z, 1.0f, dist, worldAngle);
    return Position(x, y, z, worldAngle + static_cast<float>(M_PI));
}

void DQSpawnSystem::ApplyPhase(WorldObject* obj, uint32 phaseBit)
{
    if (phaseBit)
        obj->SetPhaseMask(phaseBit, true);
}

void DQSpawnSystem::ApplyScaling(Creature* c, Player* player, const DQSpawnDesc& desc)
{
    if (desc.scaleLevel)
    {
        int32 lvl = static_cast<int32>(player->GetLevel()) + static_cast<int32>(desc.levelOffset);
        if (lvl < 1)  lvl = 1;
        if (lvl > 80) lvl = 80;
        c->SetLevel(static_cast<uint8>(lvl), false);
    }

    if (desc.hpRatio > 0.0f)
    {
        uint32 hp = static_cast<uint32>(player->GetMaxHealth() * desc.hpRatio);
        c->SetMaxHealth(hp);
        c->SetHealth(hp);
    }

    if (desc.dmgRatio > 0.0f)
    {
        float dmg = player->GetMaxHealth() * desc.dmgRatio;
        c->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, dmg * 0.8f);
        c->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, dmg * 1.2f);
        c->UpdateAttackPowerAndDamage();
    }
}

void DQSpawnSystem::ApplyFlags(Creature* c, Player* player, const DQSpawnDesc& desc)
{
    if (desc.displayId)
        c->SetDisplayId(desc.displayId, true);  // setNative=true: prevents engine revert

    if (desc.faction)
    {
        c->SetFaction(desc.faction);
        c->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }

    if (desc.attackPlayer && player)
        c->AI()->AttackStart(player);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

TempSummon* DQSpawnSystem::SpawnCourier(Player* player, const DQSpawnDesc& desc,
    float aheadDist)
{
    Position pos = CalcNearPoint(player, aheadDist, player->GetOrientation());

    if (std::abs(pos.GetPositionZ() - player->GetPositionZ()) > 10.0f)
    {
        LOG_DEBUG("module.dynamicquests",
            "DQSpawnSystem::SpawnCourier: Z delta {:.1f}y too large, aborting for player {}.",
            std::abs(pos.GetPositionZ() - player->GetPositionZ()), player->GetName());
        return nullptr;
    }

    TempSummon* summon = player->GetMap()->SummonCreature(desc.entry, pos, nullptr, 0, player);
    if (!summon)
    {
        LOG_ERROR("module.dynamicquests",
            "DQSpawnSystem::SpawnCourier: SummonCreature failed entry={} player={}.",
            desc.entry, player->GetName());
        return nullptr;
    }

    ApplyPhase(summon, desc.phaseBit);
    ApplyScaling(summon, player, desc);
    ApplyFlags(summon, player, desc);

    summon->SetWalk(true);
    summon->GetMotionMaster()->MoveFollow(player, 2.0f, static_cast<float>(M_PI));

    return summon;
}

void DQSpawnSystem::SpawnHostileRing(Player* player, uint32 count, float radius,
    const DQSpawnDesc& desc, std::vector<ObjectGuid>& outGuids)
{
    if (count == 0)
        return;

    float stepAngle = 2.0f * static_cast<float>(M_PI) / static_cast<float>(count);

    for (uint32 i = 0; i < count; ++i)
    {
        float angle = static_cast<float>(i) * stepAngle;
        Position pos = CalcNearPoint(player, radius, angle);

        TempSummon* summon = player->GetMap()->SummonCreature(
            desc.entry, pos, nullptr, 0, player);
        if (!summon)
            continue;

        ApplyPhase(summon, desc.phaseBit);
        ApplyScaling(summon, player, desc);
        ApplyFlags(summon, player, desc);

        outGuids.push_back(summon->GetGUID());
    }
}

TempSummon* DQSpawnSystem::SpawnHostileAhead(Player* player, float dist,
    const DQSpawnDesc& desc)
{
    Position pos = CalcNearPoint(player, dist, player->GetOrientation());

    TempSummon* summon = player->GetMap()->SummonCreature(
        desc.entry, pos, nullptr, 0, player);
    if (!summon)
    {
        LOG_ERROR("module.dynamicquests",
            "DQSpawnSystem::SpawnHostileAhead: SummonCreature failed entry={} player={}.",
            desc.entry, player->GetName());
        return nullptr;
    }

    ApplyPhase(summon, desc.phaseBit);
    ApplyScaling(summon, player, desc);
    ApplyFlags(summon, player, desc);

    return summon;
}

TempSummon* DQSpawnSystem::SpawnStationary(Player* player, const Position& pos,
    const DQSpawnDesc& desc)
{
    TempSummon* summon = player->GetMap()->SummonCreature(
        desc.entry, pos, nullptr, 0, player);
    if (!summon)
    {
        LOG_ERROR("module.dynamicquests",
            "DQSpawnSystem::SpawnStationary: SummonCreature failed entry={} player={}.",
            desc.entry, player->GetName());
        return nullptr;
    }

    // Apply display/faction/scaling before moving to private phase so the CREATE
    // packet the client receives (after the player's phase is updated) already
    // contains the correct model — prevents the engine's native display from
    // overriding the race-specific child model on the phase join update.
    ApplyScaling(summon, player, desc);
    ApplyFlags(summon, player, desc);
    ApplyPhase(summon, desc.phaseBit);

    return summon;
}

GameObject* DQSpawnSystem::SpawnGameObject(WorldObject* summoner, uint32 entry,
    const Position& pos, uint32 phaseBit, uint32 durationMs)
{
    GameObject* go = summoner->SummonGameObject(
        entry,
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
        pos.GetOrientation(),
        0.0f, 0.0f, 0.0f, 0.0f,
        durationMs);

    if (go && phaseBit)
        go->SetPhaseMask(phaseBit, true);

    return go;
}

TempSummon* DQSpawnSystem::SpawnWithStyle(Player* player, const DQSpawnDesc& desc,
    const std::string& style, std::vector<ObjectGuid>* outAuxGuids)
{
    if (style.empty() || style == "approaches")
        return SpawnCourier(player, desc, 22.0f);

    if (style == "distant")
        return SpawnCourier(player, desc, 55.0f);

    if (style == "run_up")
    {
        TempSummon* c = SpawnCourier(player, desc, 40.0f);
        if (c)
            c->SetSpeed(MOVE_RUN, c->GetSpeed(MOVE_RUN) * 1.8f);
        return c;
    }

    if (style == "roadside")
    {
        float fwd = player->GetOrientation();
        float lat = fwd - float(M_PI) * 0.5f;
        float px  = player->GetPositionX() + 25.0f * std::cos(fwd) + 12.0f * std::cos(lat);
        float py  = player->GetPositionY() + 25.0f * std::sin(fwd) + 12.0f * std::sin(lat);
        Position pos(px, py, player->GetPositionZ(), fwd + float(M_PI));
        return SpawnStationary(player, pos, desc);
    }

    if (style == "waiting" || style == "collapses")
        return SpawnCourier(player, desc, 2.0f);

    if (style == "from_portal")
    {
        constexpr uint32 PORTAL_GO_ENTRY = 36727;
        constexpr uint32 PORTAL_DURATION = 8; // seconds — SummonGameObject respawnDelay is in seconds
        float ang = player->GetOrientation();
        float px  = player->GetPositionX() + 6.0f * std::cos(ang);
        float py  = player->GetPositionY() + 6.0f * std::sin(ang);
        Position portalPos(px, py, player->GetPositionZ(), 0.0f);
        GameObject* portal = SpawnGameObject(player, PORTAL_GO_ENTRY, portalPos,
            desc.phaseBit, PORTAL_DURATION);
        if (portal && outAuxGuids)
            outAuxGuids->push_back(portal->GetGUID());
        return SpawnCourier(player, desc, 6.0f);
    }

    if (style == "from_shadow")
        return SpawnCourier(player, desc, 8.0f);

    LOG_WARN("module.dynamicquests",
        "DQSpawnSystem::SpawnWithStyle: unknown style '{}' — falling back to 'approaches'.", style);
    return SpawnCourier(player, desc, 22.0f);
}
