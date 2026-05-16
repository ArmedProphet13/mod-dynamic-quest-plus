/*
 * DynamicQuests+ Module
 * Phase 4: Animation System — implementation
 */

#include "DQAnimationMgr.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

DQAnimationMgr* DQAnimationMgr::instance()
{
    static DQAnimationMgr instance;
    return &instance;
}

// ---------------------------------------------------------------------------
// PlayEntry
// ---------------------------------------------------------------------------

void DQAnimationMgr::PlayEntry(Creature* npc, const std::string& style, uint32 spellId)
{
    if (!npc)
        return;

    // For all entry styles: cast the entry spell if one was authored.
    // The spell itself produces the visual — portals, fades, glows, etc.
    // "approaches" is the default motion already set by SpawnCourier (MoveFollow).
    // The other style strings are informational for authors; the spell does the work.
    if (spellId != 0)
        npc->CastSpell(npc, spellId, true);

    (void)style; // style drives spell selection at authoring time, not runtime
}

// ---------------------------------------------------------------------------
// PlayExit
// ---------------------------------------------------------------------------

void DQAnimationMgr::PlayExit(Creature* npc, const std::string& style, uint32 spellId,
                               Milliseconds delay)
{
    if (!npc)
        return;

    bool castSpell = (style == "fade_out" || style == "portal_exit" || style == "dissolve");

    if (castSpell && spellId != 0)
        npc->CastSpell(npc, spellId, true);

    npc->DespawnOrUnsummon(delay);
}

// ---------------------------------------------------------------------------
// ApplyPersistentAura / RemovePersistentAura
// ---------------------------------------------------------------------------

void DQAnimationMgr::ApplyPersistentAura(Creature* npc, uint32 spellId)
{
    if (!npc || spellId == 0)
        return;

    npc->AddAura(spellId, npc);
}

void DQAnimationMgr::RemovePersistentAura(Creature* npc, uint32 spellId)
{
    if (!npc || spellId == 0)
        return;

    npc->RemoveAurasDueToSpell(spellId);
}

// ---------------------------------------------------------------------------
// PlayBeatTransition
// ---------------------------------------------------------------------------

void DQAnimationMgr::PlayBeatTransition(Creature* npc, Player* /*player*/)
{
    if (!npc)
        return;

    npc->HandleEmoteCommand(EMOTE_ONESHOT_APPLAUD);
}
