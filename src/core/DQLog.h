/*
 * DynamicQuests+ Module
 * DQLog — structured logging layer over AzerothCore's native logger hierarchy.
 *
 * All DQ+ log output uses dot-notation subcategories under "module.dynamicquests".
 * AC's logger hierarchy walks upward automatically:
 *   module.dynamicquests.courier → module.dynamicquests → module → root
 *
 * Runtime control:
 *   .dq debug on   → sLog->SetLogLevel("module.dynamicquests", 5, true)  (DEBUG)
 *   .dq debug off  → sLog->SetLogLevel("module.dynamicquests", 4, true)  (INFO)
 *
 * Setting the parent logger level propagates to all subcategories via AC's
 * hierarchy walk — no per-subcat calls required.
 *
 * Usage:
 *   DQ_LOG_DEBUG(DQ_LOG_CAT_COURIER, player, "Courier arrived dist={:.1f}", dist);
 *   DQ_LOG_INFO (DQ_LOG_CAT_STATE,   nullptr, "State → {}", stateStr);
 *   DQ_LOG_WARN (DQ_LOG_CAT_SESSION, player, "Gossip menuId mismatch: got={} expected={}", got, exp);
 *
 * When player != nullptr, "[PlayerName] " is prepended to every message.
 */

#pragma once

#include "Log.h"
#include "Player.h"

// ---------------------------------------------------------------------------
// Subcategory constants
// ---------------------------------------------------------------------------

#define DQ_LOG_CAT_STATE    "module.dynamicquests.state"
#define DQ_LOG_CAT_COURIER  "module.dynamicquests.courier"
#define DQ_LOG_CAT_SESSION  "module.dynamicquests.session"
#define DQ_LOG_CAT_BEAT     "module.dynamicquests.beat"
#define DQ_LOG_CAT_SPAWN    "module.dynamicquests.spawn"
#define DQ_LOG_CAT_CONTEXT  "module.dynamicquests.context"

// ---------------------------------------------------------------------------
// Player name helper — returns empty string when player is null.
// Used to prefix every log message with [PlayerName] when a player is
// known, making it trivial to grep a single player's full session.
// ---------------------------------------------------------------------------

namespace DQLogDetail
{
    inline std::string PlayerPrefix(const Player* player)
    {
        if (!player)
            return {};
        return std::string("[") + player->GetName() + "] ";
    }
}

// ---------------------------------------------------------------------------
// DQ_LOG_* macros
// Each wraps the corresponding AC LOG_* macro and prepends the player prefix.
// The fmt string and variadic args are forwarded directly to AC's formatter.
// ---------------------------------------------------------------------------

#define DQ_LOG_DEBUG(cat, player, fmt, ...) \
    LOG_DEBUG(cat, "{}" fmt, DQLogDetail::PlayerPrefix(player), ##__VA_ARGS__)

#define DQ_LOG_INFO(cat, player, fmt, ...) \
    LOG_INFO(cat, "{}" fmt, DQLogDetail::PlayerPrefix(player), ##__VA_ARGS__)

#define DQ_LOG_WARN(cat, player, fmt, ...) \
    LOG_WARN(cat, "{}" fmt, DQLogDetail::PlayerPrefix(player), ##__VA_ARGS__)

#define DQ_LOG_ERROR(cat, player, fmt, ...) \
    LOG_ERROR(cat, "{}" fmt, DQLogDetail::PlayerPrefix(player), ##__VA_ARGS__)
