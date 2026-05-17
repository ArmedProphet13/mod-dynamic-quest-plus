/*
 * DynamicQuests+ Module
 * DQClientSession — central owner of all client gossip communication.
 *
 * Responsibilities:
 *   Open()    — build + send the gossip menu, fill DQPendingSession.
 *   Validate()— check that the incoming gossip select belongs to our session.
 *   Tick()    — countdown the session expiry; returns true once when it expires.
 *   Close()   — clear the session (npcGuid = Empty = no session active).
 *
 * DQPendingSession is stored in PlayerDQState (DynamicQuestMgr.h).
 * npcGuid.IsEmpty() is the canonical "no session" sentinel.
 *
 * Integration:
 *   TickApproaching (proactive) → DQClientSession::Open on courier arrival
 *   HandleGossipHello (reactive) → DQClientSession::Open on right-click
 *   HandleGossipSelect → DQClientSession::Validate then DQClientSession::Close
 *   DynamicQuestMgr::OnPlayerUpdate (DELIVERING) → DQClientSession::Tick
 *   AbortInteraction / OnPlayerLogout → DQClientSession::Close
 */

#pragma once

#include "ObjectGuid.h"

// ---------------------------------------------------------------------------
// DQPendingSession — live session record stored in PlayerDQState
// ---------------------------------------------------------------------------

struct DQPendingSession
{
    ObjectGuid npcGuid;   // creature the gossip was sent from; Empty = no session
    uint32     expiryMs;  // ms remaining before session expires

    DQPendingSession() : expiryMs(0) {}
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

class Player;
class Creature;
struct ArchetypeDef;
struct ArchetypeBeat;

// ---------------------------------------------------------------------------
// DQClientSession — static methods, no instance
// ---------------------------------------------------------------------------

class DQClientSession
{
public:
    // Build the gossip menu for the given beat, send SMSG_GOSSIP_MESSAGE,
    // and fill |out| so the response can be validated.
    // expiryMs: how long the session stays open before Tick() fires expiry (default 120s).
    static void Open(Player* player, Creature* courier,
                     const ArchetypeDef& def, const ArchetypeBeat& beat,
                     DQPendingSession& out, uint32 expiryMs = 120000);

    // Validate an incoming gossip select against the stored session.
    // Returns false (and logs DEBUG) on any mismatch — caller should discard.
    static bool Validate(Player* player, Creature* courier,
                         const DQPendingSession& session);

    // Advance the session timer by |diff| ms.
    // Returns true exactly once when the session expires; resets expiryMs to 0.
    static bool Tick(DQPendingSession& session, uint32 diff);

    // Clear the session — npcGuid = Empty = closed.
    static void Close(DQPendingSession& session);
};
