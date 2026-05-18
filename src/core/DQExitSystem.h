/*
 * DynamicQuests+ Module
 * System: Unified Exit — owns all graceful courier departures.
 *
 * DQExitSystem::Execute is the single entry point.  Callers build a DQExitDesc
 * from the current beat + InteractionInstance and pass it in; Execute routes to
 * the right primitive and the AI handles the rest autonomously.
 *
 * Exit styles:
 *   reverse    — walk back to spawn pos then despawn (default)
 *   forward    — walk 20y in NPC's facing direction then despawn
 *   away       — walk 15y directly away from player then despawn
 *   portal_out — walk back to spawn pos (portal GO is there) then despawn
 *   instant    — DespawnOrUnsummon(0) — emergency/abort path
 *   fade       — play exitSpell in place, then timed despawn
 */

#ifndef DQ_EXIT_SYSTEM_H
#define DQ_EXIT_SYSTEM_H

#include "Define.h"
#include "Position.h"
#include <string>

class Creature;
class Player;

struct DQExitDesc
{
    std::string style;             // exit style string from beat.exitStyle
    std::string animation;         // visual label from beat.exitAnimation (informational)
    uint32      spell        = 0;  // exit spell from beat.exitSpell (used by fade)
    Position    spawnPos;          // from inst.courierSpawnPos
    uint32      emoteDuration = 0; // ms: wait before starting walk (lets emote finish)
};

class DQExitSystem
{
public:
    // Route courier to the correct exit primitive based on desc.style.
    static void Execute(Creature* courier, Player* player, const DQExitDesc& desc);

private:
    static void ExitReverse  (Creature* c, const Position& dest, uint32 delayMs);
    static void ExitForward  (Creature* c, Player* p, uint32 delayMs);
    static void ExitAway     (Creature* c, Player* p, uint32 delayMs);
    static void ExitPortalOut(Creature* c, const Position& dest, uint32 delayMs);
    static void ExitInstant  (Creature* c);
    static void ExitFade     (Creature* c, uint32 spellId, uint32 emoteDuration);
};

#endif // DQ_EXIT_SYSTEM_H
