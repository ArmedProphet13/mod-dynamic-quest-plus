/*
 * DynamicQuests+ Module
 * DQSpawnSystem — centralised creature / game-object spawning primitives
 *
 * Every spawn pattern in the module funnels through one of these five methods.
 * Each handles VMAP-aware position calculation, private phase application, and
 * player-relative stat scaling so mechanics and the manager need only fill a
 * DQSpawnDesc and pick the right pattern.
 *
 * Patterns
 * ─────────
 *   SpawnCourier      — NPC aheadDist ahead of the player → MoveFollow
 *   SpawnHostileRing  — N NPCs evenly spaced at radius, each faces player
 *   SpawnHostileAhead — single NPC dist ahead, faces player, optionally attacks
 *   SpawnStationary   — NPC at a caller-supplied position, no motion assigned
 *   SpawnGameObject   — GO at a caller-supplied position (e.g. ritual portal)
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <vector>

class Creature;
class GameObject;
class Player;
class TempSummon;
class WorldObject;

// ---------------------------------------------------------------------------
// DQSpawnDesc — describes what to spawn and how to configure it.
// Zero / false is a safe no-op for every field.
// ---------------------------------------------------------------------------
struct DQSpawnDesc
{
    uint32 entry        = 0;
    uint32 phaseBit     = 0;      // 0 = no private phase applied
    float  hpRatio      = 0.0f;   // 0 = no HP override  (multiplier × player max HP)
    float  dmgRatio     = 0.0f;   // 0 = no dmg override (multiplier × player max HP per swing)
    uint32 faction      = 0;      // 0 = keep database default
    uint32 displayId    = 0;      // 0 = keep database default
    bool   scaleLevel   = false;  // true = match creature level to player level
    bool   attackPlayer = false;  // true = call AI()->AttackStart(player) after spawn
};

// ---------------------------------------------------------------------------
// DQSpawnSystem — static factory; no instance required.
// ---------------------------------------------------------------------------
class DQSpawnSystem
{
public:
    // Courier: spawns aheadDist ahead of the player, faces the player, then
    // starts MoveFollow.  Returns nullptr if the Z delta exceeds 10 y (cliff /
    // terrain abort) or if SummonCreature fails.
    static TempSummon* SpawnCourier(Player* player, const DQSpawnDesc& desc,
        float aheadDist = 22.0f);

    // Hostile ring: spawns count creatures evenly spaced at radius around the
    // player.  Each faces the player.  GUIDs are appended to outGuids.
    static void SpawnHostileRing(Player* player, uint32 count, float radius,
        const DQSpawnDesc& desc, std::vector<ObjectGuid>& outGuids);

    // Hostile ahead: spawns one creature dist ahead of the player, facing the
    // player.  Returns nullptr on failure.
    static TempSummon* SpawnHostileAhead(Player* player, float dist,
        const DQSpawnDesc& desc);

    // Stationary: spawns one creature at pos with no motion assigned.
    // Returns nullptr on failure.
    static TempSummon* SpawnStationary(Player* player, const Position& pos,
        const DQSpawnDesc& desc);

    // GameObject: summons a GO at pos, then optionally phases it.
    // summoner may be a Player or a Creature (WorldObject suffices).
    static GameObject* SpawnGameObject(WorldObject* summoner, uint32 entry,
        const Position& pos, uint32 phaseBit = 0, uint32 durationMs = 300000);

private:
    // VMAP-aware position at worldAngle radians from the player, dist away,
    // with facing set inward (worldAngle + π).
    static Position CalcNearPoint(Player* player, float dist, float worldAngle);

    static void ApplyPhase(WorldObject* obj, uint32 phaseBit);
    static void ApplyScaling(Creature* c, Player* player, const DQSpawnDesc& desc);
    static void ApplyFlags(Creature* c, Player* player, const DQSpawnDesc& desc);
};
