/*
 * DynamicQuests+ Module
 * Phase 6: Dialogue System
 *
 * Owns the gossip window completely.  All gossip menus presented to the player
 * are built here from ArchetypeBeat data -- no hardcoded branches, no legacy
 * InteractionTemplate lookups.
 *
 * Responsibilities:
 *   OpenBeatGossip    -- builds and sends the gossip menu for the current beat
 *   GetVariantText    -- pulls a random fallback line from dq_text_variant_pool
 *   LoadFromDB        -- caches dq_text_variant_pool at startup
 *
 * Gossip action IDs used internally:
 *   0-3               -- branching choice index (BRANCHING pattern, up to 4 buttons)
 *   0                 -- single accept action for all non-branching active mechanics
 *   DQ_GOSSIP_DECLINE -- player walks away without accepting
 *
 * 0xFE is chosen for decline to avoid collision with:
 *   branching choice indices 0-3 and the active accept action 0
 */

#ifndef DQ_DIALOGUE_MGR_H
#define DQ_DIALOGUE_MGR_H

#include "ArchetypeMgr.h"
#include <string>
#include <vector>

class Creature;
class Player;

// Gossip sender used for all DQ+ menu items.
static constexpr uint32 DQ_COURIER_SENDER   = 1203;
// NPC text with blank body -- NPC already spoke via Say() on arrive.
static constexpr uint32 GOSSIP_TEXT_COURIER = 9000003;
// Walk-away action (0xFE avoids collision with branching choice indices 0-3).
static constexpr uint32 DQ_GOSSIP_DECLINE   = 0xFE;

class DQDialogueMgr
{
public:
    static DQDialogueMgr* instance();

    // Load dq_text_variant_pool from WorldDatabase.  Called on startup.
    void LoadFromDB();

    // Build and send the gossip menu for the active beat.
    // Uses beat mechanic + passive flag to decide which buttons appear.
    void OpenBeatGossip(Player* player, Creature* npc,
                        const ArchetypeDef& def, const ArchetypeBeat& beat);

    // Return a random text line from the pool matching emotion + textType.
    // contextTags is optional and used as a bonus filter; falls back to
    // emotion + textType only when no context-specific match exists.
    std::string GetVariantText(const std::string& emotion,
                               const std::string& contextTags,
                               const std::string& textType) const;

    bool IsLoaded() const { return _loaded; }

private:
    DQDialogueMgr() = default;

    struct TextVariant
    {
        std::string emotion;
        std::string contextTags;
        std::string textType;
        std::string text;
    };

    std::vector<TextVariant> _variants;
    bool _loaded = false;
};

#define sDQDialogue DQDialogueMgr::instance()

#endif // DQ_DIALOGUE_MGR_H
