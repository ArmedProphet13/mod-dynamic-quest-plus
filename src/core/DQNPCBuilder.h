/*
 * DynamicQuests+ Module
 * Phase 2: NPC Builder
 *
 * Centralises all NPC profile construction for the archetype engine.
 * Takes an ArchetypeDef + ArchetypeBeat + Player and produces a DQNPCProfile
 * ready to pass directly to DQSpawnSystem.
 *
 * Also owns the three mid-quest mutations that modify a living creature:
 *   FlipToHostile  — used by the Fight Handler when combat begins
 *   FlipToFriendly — used by the Fight Handler on concede / abort
 *   SetHPPercent   — used by cost effects and fight threshold adjustments
 *
 * This class never spawns anything — spawning is always DQSpawnSystem's job.
 */

#ifndef DQ_NPC_BUILDER_H
#define DQ_NPC_BUILDER_H

#include "ArchetypeMgr.h"
#include "DQSpawnSystem.h"

class Creature;
class Player;

// ---------------------------------------------------------------------------
// DQNPCProfile — fully resolved spawn descriptor produced for one beat
// ---------------------------------------------------------------------------

struct DQNPCProfile
{
    DQSpawnDesc spawnDesc;       // feed directly into DQSpawnSystem::Spawn*
    uint8       concedePct  = 15; // HP% below which a fight NPC concedes (beat.fightThreshold)
    uint32      entrySpell  = 0;  // cast on NPC immediately after spawn  (beat.entrySpell)
    uint32      exitSpell   = 0;  // cast on NPC just before despawn       (beat.exitSpell)
    uint32      auraSpell   = 0;  // persistent aura applied for beat duration (beat.auraSpell)
};

// ---------------------------------------------------------------------------
// DQNPCBuilder — stateless factory + mid-quest mutator
// ---------------------------------------------------------------------------

class DQNPCBuilder
{
public:
    // Build a complete NPC profile from archetype def + beat + player.
    // Resolves the creature entry via WorldCatalogue (appearance tags),
    // applies level offset, and fills all DQSpawnDesc fields.
    // Does NOT spawn — call DQSpawnSystem after.
    static DQNPCProfile BuildProfile(const ArchetypeDef& def,
                                     const ArchetypeBeat& beat,
                                     Player* player);

    // Mid-quest: turn a living NPC hostile and start combat with the player.
    // Called by the Fight Handler when the fight phase begins.
    static void FlipToHostile(Creature* npc, Player* player);

    // Mid-quest: stop combat and return a living NPC to neutral/friendly.
    // Called by the Fight Handler on concede, or by Resolution on abort.
    static void FlipToFriendly(Creature* npc);

    // Mid-quest: set NPC health to exactly percent% of their current maximum.
    // Clamped to [1 HP, max HP].  percent == 0 is treated as a no-op.
    static void SetHPPercent(Creature* npc, uint8 percent);

private:
    // Parse a comma-separated appearance string into a tag list.
    static void ParseAppearanceTags(const std::string& appearance,
                                    std::vector<std::string>& out);
};

#endif // DQ_NPC_BUILDER_H
