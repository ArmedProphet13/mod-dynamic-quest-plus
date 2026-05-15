/*
 * DynamicQuests+ Module
 * Mechanic: Succubus — Implementation
 */

#include "MechanicSuccubus.h"
#include "DynamicQuestMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Log.h"
#include "Player.h"

// Forward declaration — DQ_SuccubusAI exposes BeginAcceptSequence and BeginCombat
// through the CreatureAI pointer; we cast via CAST_AI inside OnChoice.
// Including the AI header here would create a circular dependency, so we access it
// via the public method pointers declared in DQ_SuccubusAI.h instead.
#include "DQ_SuccubusAI.h"

// ── Reward item IDs ───────────────────────────────────────────────────────────
static constexpr uint32 ITEM_BLUE_ARMOR_CACHE   = 900101;
static constexpr uint32 ITEM_BLUE_WEAPON_CACHE  = 900104;
static constexpr uint32 ITEM_PURPLE_WEAPON_CACHE = 900105;

// ── IMechanicModule interface ─────────────────────────────────────────────────

void MechanicSuccubus::OnStart(Player* /*player*/, Creature* /*courier*/, InteractionInstance& /*inst*/)
{
    // Intentionally empty — DQ_SuccubusAI drives the encounter from its own
    // phase state machine starting the moment the creature spawns.
}

void MechanicSuccubus::OnChoice(Player* player, uint32 choiceId, InteractionInstance& inst)
{
    Creature* ileana = !inst.courierGuid.IsEmpty()
        ? player->GetMap()->GetCreature(inst.courierGuid) : nullptr;

    if (!ileana)
    {
        OnFail(player, inst);
        return;
    }

    if (choiceId == 0) // Accept
    {
        if (DQ_SuccubusAI* ai = CAST_AI(DQ_SuccubusAI, ileana->AI()))
            ai->BeginAcceptSequence(player);
    }
    else // Fight (choiceId == 1)
    {
        if (DQ_SuccubusAI* ai = CAST_AI(DQ_SuccubusAI, ileana->AI()))
            ai->BeginCombat(player);
    }
}

void MechanicSuccubus::OnKill(Player* player, Creature* victim, InteractionInstance& inst)
{
    if (victim->GetGUID() != inst.courierGuid)
        return;

    // Determine episode from template ID (10=ep1, 11=ep2, 12=ep3)
    uint8 episode = static_cast<uint8>(inst.templateId - 9); // 10→1, 11→2, 12→3
    if (episode < 1 || episode > 3)
        episode = 1;

    // Fight reward: Blue Weapon for ep1, both caches for ep2, Purple Weapon for ep3
    switch (episode)
    {
        case 1:
            player->AddItem(ITEM_BLUE_WEAPON_CACHE, 1);
            break;
        case 2:
            player->AddItem(ITEM_BLUE_ARMOR_CACHE,  1);
            player->AddItem(ITEM_BLUE_WEAPON_CACHE,  1);
            break;
        case 3:
            player->AddItem(ITEM_PURPLE_WEAPON_CACHE, 1);
            break;
    }

    LOG_INFO("module.dynamicquests.mechanic",
        "MechanicSuccubus: player {} killed Ileana (ep{}). Fight reward delivered.",
        player->GetName(), episode);

    inst.courierGuid = ObjectGuid::Empty;
    OnComplete(player, inst);
}

void MechanicSuccubus::OnComplete(Player* player, InteractionInstance& inst)
{
    inst.completed = true;
    sDQMgr->OnInteractionComplete(player);
}

void MechanicSuccubus::OnFail(Player* player, InteractionInstance& inst)
{
    if (!inst.courierGuid.IsEmpty())
        if (Creature* ileana = player->GetMap()->GetCreature(inst.courierGuid))
            ileana->DespawnOrUnsummon(Milliseconds(0));

    inst.courierGuid = ObjectGuid::Empty;
    inst.failed = true;
    sDQMgr->OnInteractionFailed(player);
}

void MechanicSuccubus::OnCleanup(Player* /*player*/, InteractionInstance& inst)
{
    inst.courierGuid = ObjectGuid::Empty;
}

// ── Static helper ─────────────────────────────────────────────────────────────

void MechanicSuccubus::DeliverAcceptReward(Player* player, uint8 episode)
{
    switch (episode)
    {
        case 1:
            player->AddItem(ITEM_BLUE_ARMOR_CACHE, 1);
            break;
        case 2:
            player->AddItem(ITEM_BLUE_ARMOR_CACHE,  1);
            player->AddItem(ITEM_BLUE_WEAPON_CACHE,  1);
            break;
        case 3:
        default:
            player->AddItem(ITEM_PURPLE_WEAPON_CACHE, 1);
            break;
    }

    LOG_INFO("module.dynamicquests.mechanic",
        "MechanicSuccubus: accept reward delivered to {} (ep{}).",
        player->GetName(), episode);
}
