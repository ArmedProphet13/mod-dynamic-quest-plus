/*
 * DynamicQuests+ Module
 * Phase 4: Animation System
 *
 * Owns everything visual that is not a model or a stat.
 * Called by MechanicArchetype at three points in the beat lifecycle:
 *   PlayEntry       — immediately after the courier is received in OnStart
 *   PlayExit        — before the courier despawns in CompleteBeat / OnCleanup
 *   PlayBeatTransition — brief objective-complete visual cue
 *
 * Animation styles are authored as strings in dq_archetype_beat columns
 * entry_animation / exit_animation.  Spell IDs are sourced from dq_dummy_spell
 * and stored directly in beat.entrySpell / beat.exitSpell / beat.auraSpell.
 *
 * The system never makes routing decisions — it only performs visual effects
 * on the creature it receives.
 */

#ifndef DQ_ANIMATION_MGR_H
#define DQ_ANIMATION_MGR_H

#include "Define.h"
#include "Duration.h"
#include <string>

class Creature;
class Player;

class DQAnimationMgr
{
public:
    static DQAnimationMgr* instance();

    // Entry: cast entrySpell on the NPC according to the named animation style.
    // "approaches"  — MoveFollow already set by SpawnCourier; cast spell if non-zero
    // "from_portal" — cast spell (portal visual plays immediately)
    // "fade_in"     — cast spell (fade visual)
    // "materialize" — cast spell
    // (any other)   — cast spell if non-zero, no-op otherwise
    void PlayEntry(Creature* npc, const std::string& style, uint32 spellId);

    // Exit: cast exitSpell then schedule despawn after delay.
    // "despawn"     — DespawnOrUnsummon only (no spell)
    // "fade_out"    — cast spell, then despawn
    // "portal_exit" — cast spell, then despawn
    // "dissolve"    — cast spell, then despawn
    // (any other)   — DespawnOrUnsummon only
    void PlayExit(Creature* npc, const std::string& style, uint32 spellId,
                  Milliseconds delay = Milliseconds(2500));

    // Apply a persistent aura to a living NPC for the duration of the beat.
    void ApplyPersistentAura(Creature* npc, uint32 spellId);

    // Remove the persistent aura before the NPC despawns.
    void RemovePersistentAura(Creature* npc, uint32 spellId);

    // Brief objective-complete visual: plays EMOTE_ONESHOT_APPLAUD on the NPC.
    void PlayBeatTransition(Creature* npc, Player* player);

private:
    DQAnimationMgr() = default;
};

#define sDQAnimation DQAnimationMgr::instance()

#endif // DQ_ANIMATION_MGR_H
