/*
 * DynamicQuests+ Module
 * System: Unified Exit — implementation
 */

#include "DQExitSystem.h"
#include "DQ_CourierAI.h"
#include "DQAnimationMgr.h"
#include "Creature.h"
#include "Duration.h"
#include "Player.h"
#include <cmath>

void DQExitSystem::Execute(Creature* courier, Player* player, const DQExitDesc& desc)
{
    if (!courier)
        return;

    if (desc.style == "instant")
        { ExitInstant(courier);                                      return; }
    if (desc.style == "fade")
        { ExitFade(courier, desc.spell, desc.emoteDuration);         return; }
    if (desc.style == "away")
        { ExitAway(courier, player, desc.emoteDuration);             return; }
    if (desc.style == "forward")
        { ExitForward(courier, player, desc.emoteDuration);          return; }
    if (desc.style == "portal_out")
        { ExitPortalOut(courier, desc.spawnPos, desc.emoteDuration); return; }

    // Default: reverse — walk back to spawn position.
    ExitReverse(courier, desc.spawnPos, desc.emoteDuration);
}

void DQExitSystem::ExitReverse(Creature* c, const Position& dest, uint32 delayMs)
{
    DQCourierBeginDeparture(c, dest, delayMs);
}

void DQExitSystem::ExitForward(Creature* c, Player* /*p*/, uint32 delayMs)
{
    float angle = c->GetOrientation();
    Position dest(
        c->GetPositionX() + 20.f * std::cos(angle),
        c->GetPositionY() + 20.f * std::sin(angle),
        c->GetPositionZ());
    DQCourierBeginDeparture(c, dest, delayMs);
}

void DQExitSystem::ExitAway(Creature* c, Player* p, uint32 delayMs)
{
    float angle = p ? p->GetAngle(c) : c->GetOrientation();
    Position dest(
        c->GetPositionX() + 15.f * std::cos(angle),
        c->GetPositionY() + 15.f * std::sin(angle),
        c->GetPositionZ());
    DQCourierBeginDeparture(c, dest, delayMs);
}

void DQExitSystem::ExitPortalOut(Creature* c, const Position& dest, uint32 delayMs)
{
    DQCourierBeginDeparture(c, dest, delayMs);
}

void DQExitSystem::ExitInstant(Creature* c)
{
    c->DespawnOrUnsummon(Milliseconds(0));
}

void DQExitSystem::ExitFade(Creature* c, uint32 spellId, uint32 emoteDuration)
{
    sDQAnimation->PlayExitSpell(c, spellId);
    c->DespawnOrUnsummon(Milliseconds(emoteDuration + 2000));
}
