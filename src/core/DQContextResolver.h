/*
 * DynamicQuests+ Module
 * System 5: Position & Context Resolver
 *
 * Resolves a flat tag set describing who a player is and where they are.
 * Called once per TryTrigger attempt — not every tick.
 *
 * Author contract: archetypes declare required_tags / excluded_tags as
 * comma-separated strings. Empty = no restriction (fires for everyone).
 *
 * Tag sources (in resolution order):
 *   1. Player properties  — class, race, faction, state (zero DB)
 *   2. Map / area flags   — city, sanctuary, pvp_zone, instance types (DBC)
 *   3. Custom area tags   — dq_area_tags table, keyed by area_id
 *   4. Proximity tags     — graveyard_nearby from game_graveyard; dq_landmarks
 *   5. Hierarchy expansion— parent tags added via dq_tag_hierarchy
 *
 * All relationships between tags are defined in dq_tag_hierarchy. No
 * hardcoded hierarchies exist in C++ — add new ones with a single SQL INSERT.
 */

#ifndef DQ_CONTEXT_RESOLVER_H
#define DQ_CONTEXT_RESOLVER_H

#include "Define.h"
#include "PlayerContextObserver.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Player;

// ---------------------------------------------------------------------------
// DQTagSet — flat set of active string tags for one player at one moment
// ---------------------------------------------------------------------------
using DQTagSet = std::unordered_set<std::string>;

// ---------------------------------------------------------------------------
// DQContextResolver — singleton
// ---------------------------------------------------------------------------
class DQContextResolver
{
public:
    static DQContextResolver* instance();

    // Called once from DynamicQuestMgr::Initialize(). Loads dq_area_tags,
    // dq_tag_hierarchy, and dq_landmarks from WorldDatabase.
    void Initialize();

    // Resolve the full tag set for a player at their current position.
    // Calls all 5 resolution steps and returns the expanded set.
    DQTagSet Resolve(Player* player, const PlayerContext& ctx) const;

    // Returns true if all required tags are present in tags AND none of
    // the excluded tags are present. Empty vectors = no restriction.
    bool PassesFilter(const DQTagSet& tags,
                      const std::vector<std::string>& required,
                      const std::vector<std::string>& excluded) const;

    // Returns a sorted, comma-separated string of all tags — for .dq context.
    std::string DebugDump(const DQTagSet& tags) const;

private:
    DQContextResolver() = default;

    // Loaded at startup
    std::unordered_map<uint32, std::vector<std::string>> _areaTags;  // area_id → tag list
    std::unordered_map<std::string, std::vector<std::string>> _parents; // child → parent list

    struct GraveyardPos { float x, y, z; };
    std::unordered_map<uint32, std::vector<GraveyardPos>> _graveyards; // map_id → positions

    struct Landmark { uint32 mapId; float x, y, z, radius; std::string tag; };
    std::vector<Landmark> _landmarks;

    // Step 5: walk _parents recursively and add ancestor tags
    void ExpandHierarchy(DQTagSet& tags) const;

    // Step 4: distance checks against cached graveyard positions + dq_landmarks
    void AddProximityTags(Player* player, DQTagSet& tags) const;
};

#define sDQContext DQContextResolver::instance()

#endif // DQ_CONTEXT_RESOLVER_H
