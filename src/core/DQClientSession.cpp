/*
 * DynamicQuests+ Module
 * DQClientSession — implementation
 */

#include "DQClientSession.h"
#include "DQDialogueMgr.h"
#include "DQLog.h"
#include "Creature.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include <atomic>

// ---------------------------------------------------------------------------
// NextMenuId — monotonic, skips 0, wraps at 0xFFFF
// ---------------------------------------------------------------------------

uint32 DQClientSession::NextMenuId()
{
    static std::atomic<uint32> counter{ 0 };
    uint32 next;
    do
    {
        next = (counter.fetch_add(1, std::memory_order_relaxed) + 1) & 0xFFFFu;
    }
    while (next == 0);
    return next;
}

// ---------------------------------------------------------------------------
// Open
// ---------------------------------------------------------------------------

void DQClientSession::Open(Player* player, Creature* courier,
                           const ArchetypeDef& def, const ArchetypeBeat& beat,
                           DQPendingSession& out, uint32 expiryMs)
{
    if (!player || !courier)
        return;

    // Build gossip items — ClearGossipMenuFor + AddGossipItemFor only (no send yet).
    sDQDialogue->BuildBeatMenu(player, def, beat);

    // Stamp a unique non-zero menuId BEFORE sending so the packet carries it.
    uint32 menuId = NextMenuId();
    player->PlayerTalkClass->GetGossipMenu().SetMenuId(menuId);

    SendGossipMenuFor(player, GOSSIP_TEXT_COURIER, courier);

    out.npcGuid  = courier->GetGUID();
    out.menuId   = menuId;
    out.expiryMs = expiryMs;

    DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "Session opened. menuId={} expiryMs={}", menuId, expiryMs);
}

// ---------------------------------------------------------------------------
// Validate
// ---------------------------------------------------------------------------

bool DQClientSession::Validate(Player* player, Creature* courier,
                               uint32 incomingMenuId, const DQPendingSession& session)
{
    if (session.menuId == 0)
    {
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "Validate: no active session (menuId=0).");
        return false;
    }

    if (courier && session.npcGuid != courier->GetGUID())
    {
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "Validate: npcGuid mismatch. session={} incoming={}",
            session.npcGuid.GetRawValue(), courier->GetGUID().GetRawValue());
        return false;
    }

    if (incomingMenuId != session.menuId)
    {
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "Validate: menuId mismatch. sent={} received={}",
            session.menuId, incomingMenuId);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

bool DQClientSession::Tick(DQPendingSession& session, uint32 diff)
{
    if (session.menuId == 0)
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
    session.menuId   = 0;
    session.expiryMs = 0;
}
