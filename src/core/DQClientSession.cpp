/*
 * DynamicQuests+ Module
 * DQClientSession — implementation
 */

#include "DQClientSession.h"
#include "DQDialogueMgr.h"
#include "DQLog.h"
#include "Creature.h"
#include "Player.h"
#include "ScriptedGossip.h"

// ---------------------------------------------------------------------------
// Open
// ---------------------------------------------------------------------------

void DQClientSession::Open(Player* player, Creature* courier,
                           const ArchetypeDef& def, const ArchetypeBeat& beat,
                           DQPendingSession& out, uint32 expiryMs)
{
    if (!player || !courier)
        return;

    // menuId stays 0 (standard AC creature gossip). textId=1 ("Greetings, $N") — the NPC
    // already delivered its greeting via Say() on arrive; the gossip body carries the options.
    sDQDialogue->BuildBeatMenu(player, def, beat);
    SendGossipMenuFor(player, 1, courier);

    out.npcGuid  = courier->GetGUID();
    out.expiryMs = expiryMs;

    DQ_LOG_INFO(DQ_LOG_CAT_SESSION, player, "Session opened. expiryMs={}", expiryMs);
}

// ---------------------------------------------------------------------------
// Validate
// ---------------------------------------------------------------------------

bool DQClientSession::Validate(Player* player, Creature* courier,
                               const DQPendingSession& session)
{
    if (session.npcGuid.IsEmpty())
    {
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "Validate: no active session.");
        return false;
    }

    if (courier && session.npcGuid != courier->GetGUID())
    {
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "Validate: npcGuid mismatch. session={} incoming={}",
            session.npcGuid.GetRawValue(), courier->GetGUID().GetRawValue());
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

bool DQClientSession::Tick(DQPendingSession& session, uint32 diff)
{
    if (session.npcGuid.IsEmpty())
        return false;

    if (session.expiryMs <= diff)
    {
        session.expiryMs = 0;
        return true; // expired
    }

    session.expiryMs -= diff;
    return false;
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------

void DQClientSession::Close(DQPendingSession& session)
{
    session.npcGuid  = ObjectGuid::Empty;
    session.expiryMs = 0;
}
