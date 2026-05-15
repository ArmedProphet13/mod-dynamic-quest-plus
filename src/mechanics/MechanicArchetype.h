/*
 * DynamicQuests+ Module
 * Mechanic: Archetype — data-driven multi-beat story engine
 *
 * A single IMechanicModule that handles all three beat-progression patterns
 * (Sequential, Counter, Branching). Which story plays is determined by
 * inst.templateId (decoded via DecodeArchetypeId); the beat logic is
 * fully driven by rows in dq_archetype / dq_archetype_beat.
 */

#ifndef DQ_MECHANIC_ARCHETYPE_H
#define DQ_MECHANIC_ARCHETYPE_H

#include "IMechanicModule.h"

class Player;
class Creature;

class MechanicArchetype : public IMechanicModule
{
public:
    // Called when courier arrives; applies display_id override, says greeting, plays arrive emote.
    void OnStart(Player* player, Creature* courier, InteractionInstance& inst) override;

    // Called when player selects gossip choice. Handles encounter_count and branching.
    void OnChoice(Player* player, uint32 choiceId, InteractionInstance& inst) override;

    // Not called by DynamicQuestMgr in current flow; here for interface completeness.
    void OnComplete(Player* player, InteractionInstance& inst) override;

    // Clears any beat progress flags; beat state in DB is not rolled back.
    void OnFail(Player* player, InteractionInstance& inst) override;

    // Despawns courier if still alive.
    void OnCleanup(Player* player, InteractionInstance& inst) override;

    // Drives timer-based transitions.
    void OnUpdate(Player* player, uint32 diff, InteractionInstance& inst) override;

    // Drives kill-objective beats.
    void OnKill(Player* player, Creature* victim, InteractionInstance& inst) override;

    // GM command: force-advance to next beat regardless of progress.
    void ForceAdvance(Player* player, InteractionInstance& inst) override;

    const char* GetName() const override { return "MechanicArchetype"; }

private:
    // Reads current_beat and beat_count from character_dq_sequences.
    // Returns defaults (beat=1, count=0, completed=false) if no row exists.
    static void LoadBeatState(uint32 guid, uint32 archetypeId,
        uint8& outBeat, uint16& outCount, bool& outCompleted);

    // Writes the new beat state to character_dq_sequences (async, fire-and-forget).
    static void SaveBeatState(uint32 guid, uint32 archetypeId,
        uint8 currentBeat, uint16 beatCount, bool completed);

    // Computes and persists the next beat state after the current beat completes.
    // Sets inst.completed = true if the archetype is now done.
    void AdvanceBeat(Player* player, InteractionInstance& inst);

    // Common completion path: play complete emote, advance beat, notify manager.
    void CompleteBeat(Player* player, InteractionInstance& inst);
};

#endif // DQ_MECHANIC_ARCHETYPE_H
