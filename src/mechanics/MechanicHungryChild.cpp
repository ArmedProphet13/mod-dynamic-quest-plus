/*
 * DynamicQuests+ Module
 * Mechanic: HungryChild — Implementation
 *
 * The AI (DQ_HungryChildAI) drives the full encounter.
 * OnInteractionAccepted/OnChoice are never called — the AI calls
 * sDQMgr->OnInteractionComplete / OnInteractionDeclined / OnInteractionFailed
 * directly after handling the gossip selection itself.
 *
 * This mechanic only needs to clean up if the interaction is aborted
 * externally (player logout, .dq stop, zone change timeout).
 */

#include "MechanicHungryChild.h"
#include "DynamicQuestMgr.h"
#include "Creature.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "SharedDefines.h"

uint32 MechanicHungryChild::GetChildDisplayId(Player* player)
{
    // Verified humanoid child display IDs from creature_template_model (WoTLK 3.3.5a):
    //   338   — Human child    (entry 14305, "Human Orphan")
    //   14589 — Orc child      (entry 14444, "Orcish Orphan")
    //   19315 — Blood Elf child (entry 22817, "Blood Elf Orphan")
    //
    // Races with no humanoid child model get the faction-default fallback.
    // Draenei child models look beast-like — use human fallback instead.
    // Undead has no child model — use orc fallback.
    // Returns 0 only for unknown races (quest will be skipped).
    switch (player->getRace())
    {
        // Alliance — human child as default fallback
        case RACE_HUMAN:    return 338;
        case RACE_DWARF:    return 338;
        case RACE_NIGHTELF: return 338;
        case RACE_GNOME:    return 338;
        case RACE_DRAENEI:  return 338;   // beast-like model in AC — use human
        // Horde — orc child as default fallback
        case RACE_ORC:            return 14589;
        case RACE_UNDEAD_PLAYER:  return 14589;
        case RACE_TAUREN:         return 14589;
        case RACE_TROLL:          return 14589;
        case RACE_BLOODELF:       return 19315;
        default:                  return 0;   // unknown race — skip quest
    }
}

void MechanicHungryChild::OnStart(Player* player, Creature* /*courier*/, InteractionInstance& /*inst*/)
{
    LOG_DEBUG("module.dynamicquests.mechanic", "HungryChild: OnStart player={}", player->GetName());
}

void MechanicHungryChild::OnChoice(Player* /*player*/, uint32 /*choiceId*/, InteractionInstance& /*inst*/)
{
    // Never called — the AI handles gossip selection directly.
}

void MechanicHungryChild::OnComplete(Player* /*player*/, InteractionInstance& /*inst*/)
{
    // Never called — the AI calls sDQMgr->OnInteractionComplete directly.
}

void MechanicHungryChild::OnFail(Player* player, InteractionInstance& inst)
{
    if (!inst.courierGuid.IsEmpty())
        if (Creature* c = player->GetMap()->GetCreature(inst.courierGuid))
            c->DespawnOrUnsummon(Milliseconds(0));
    inst.courierGuid = ObjectGuid::Empty;
    inst.failed = true;
}

void MechanicHungryChild::OnCleanup(Player* player, InteractionInstance& inst)
{
    if (!inst.courierGuid.IsEmpty())
        if (Creature* c = player->GetMap()->GetCreature(inst.courierGuid))
            c->DespawnOrUnsummon(Milliseconds(0));
    inst.courierGuid = ObjectGuid::Empty;
}
