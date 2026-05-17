/*
 * DynamicQuests+ Module
 * DQClientSession — central owner of all client gossip communication.
 *
 * Responsibilities:
 *   Open()    — build + send the gossip menu, stamp a unique non-zero menuId,
 *               fill DQPendingSession so we can validate the response.
 *   Validate()— check that the incoming gossip select matches what we sent.
 *   Tick()    — countdown the session expiry; returns true once when it expires.
 *   Close()   — zero the session (menuId=0 = no session active).
 *
 * DQPendingSession is stored in PlayerDQState (DynamicQuestMgr.h).
 * menuId == 0 is the canonical "no session" sentinel — no extra bool needed.
 *
 * Integration:
 *   HandleGossipHello → DQClientSession::Open
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
    ObjectGuid npcGuid;   // creature the gossip was sent from
    uint32     menuId;    // non-zero = session active; 0 = no session
    uint32     expiryMs;  // ms remaining before session expires

    DQPendingSession() : menuId(0), expiryMs(0) {}
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
    // Build the gossip menu for the given beat, stamp a unique non-zero menuId,
    // send SMSG_GOSSIP_MESSAGE, and fill |out| so the response can be validated.
    // expiryMs: how long the session stays open before Tick() fires expiry (default 120s).
    static void Open(Player* player, Creature* courier,
                     const ArchetypeDef& def, const ArchetypeBeat& beat,
                     DQPendingSession& out, uint32 expiryMs = 120000);

    // Validate an incoming gossip select against the stored session.
    // Returns false (and logs DEBUG) on any mismatch — caller should discard.
    static bool Validate(Player* player, Creature* courier,
                         uint32 incomingMenuId, const DQPendingSession& session);

    // Advance the session timer by |diff| ms.
    // Returns true exactly once when the session expires; resets expiryMs to 0.
    static bool Tick(DQPendingSession& session, uint32 diff);

    // Zero the session — menuId = 0 = closed.
    static void Close(DQPendingSession& session);

private:
    // Monotonic counter: skips 0, wraps at 0xFFFF (fits WoW gossip menuId field).
    static uint32 NextMenuId();
};
