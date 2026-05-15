/*
 * DynamicQuests+ Module
 * System 7: Archetype Engine — data-driven multi-beat story manager
 *
 * All six story archetypes reduce to three coded state-machine patterns.
 * New stories are authored as SQL rows only — no C++ required per story.
 *
 * Encoding convention for template IDs:
 *   Regular interaction template : id as-is (< 0x80000000)
 *   Archetype interaction        : archetype_id | 0x80000000
 *   Use EncodeArchetypeId / DecodeArchetypeId / IsArchetypeId helpers.
 */

#ifndef DQ_ARCHETYPE_MGR_H
#define DQ_ARCHETYPE_MGR_H

#include "Define.h"
#include <string>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Enums matching the dq_archetype / dq_archetype_beat ENUM columns
// ---------------------------------------------------------------------------

enum DQPattern : uint8
{
    DQ_PATTERN_SEQUENTIAL = 0, // beats 1→2→…→N in order
    DQ_PATTERN_COUNTER    = 1, // beat 1 repeats N times, then final beat fires
    DQ_PATTERN_BRANCHING  = 2, // player choice on beat 1 determines next beat
};

enum DQBeatMechanic : uint8
{
    DQ_BEAT_WITNESS  = 0, // player is present; NPC performs a scene
    DQ_BEAT_COURIER  = 1, // player carries an item from A to B
    DQ_BEAT_GOTO     = 2, // player reaches a specific position
    DQ_BEAT_KILL     = 3, // a specific creature entry is killed
    DQ_BEAT_ACTIVATE = 4, // player uses/clicks a gameobject
};

enum DQBeatTransition : uint8
{
    DQ_TRANS_QUEST_COMPLETE  = 0, // mechanic objective fulfilled
    DQ_TRANS_ENCOUNTER_COUNT = 1, // player enters NPC range N times (transition_value = N)
    DQ_TRANS_CHOICE          = 2, // player selects a gossip choice (branching)
    DQ_TRANS_TIMER           = 3, // N seconds of NPC presence (transition_value = N)
};

// ---------------------------------------------------------------------------
// In-memory beat descriptor
// ---------------------------------------------------------------------------

struct ArchetypeBeat
{
    uint32           archetypeId     = 0;
    uint8            beatNumber      = 1;
    uint32           displayId       = 0;   // 0 = use archetype appearance tags; non-zero = explicit override
    uint32           zoneId          = 0;   // 0 = inherit from ArchetypeDef
    DQBeatMechanic   mechanic        = DQ_BEAT_WITNESS;
    DQBeatTransition transitionType  = DQ_TRANS_QUEST_COMPLETE;
    uint16           transitionValue = 1;   // N for encounter_count; seconds for timer
    std::string      textGreeting;          // NPC opening line on spawn
    std::string      textChase;             // NPC line when player walks away
    int16            emoteOnArrive   = 0;   // emote played immediately on spawn (0 = none)
    int16            emoteOnComplete = 0;   // emote played when beat completes (0 = none)
    std::string      rewardPool;            // dq_reward_pool.pool_name; empty = no reward
    // System 1: Spawn Style
    std::string      spawnStyle      = "approaches"; // how the courier enters the scene
    // System 3: Prop Spawning
    uint32           propEntry       = 0;   // GO entry to scatter (0 = no props)
    uint8            propCount       = 1;   // how many GOs to scatter
    float            propRadius      = 12.f; // scatter radius (yards)
};

// ---------------------------------------------------------------------------
// In-memory archetype descriptor
// ---------------------------------------------------------------------------

struct ArchetypeDef
{
    uint32      id         = 0;
    std::string name;
    DQPattern   pattern    = DQ_PATTERN_SEQUENTIAL;
    uint8       totalBeats = 1;
    uint32      zoneId     = 0;   // 0 = any zone
    uint8       levelMin   = 1;
    uint8       levelMax   = 80;
    bool        enabled    = true;
    // System 2: Appearance — tag expression for NPC model selection ("humanoid,male,peasant")
    std::string appearance;

    std::vector<ArchetypeBeat> beats;
};

// ---------------------------------------------------------------------------
// ArchetypeMgr — singleton, loaded once at startup from WorldDatabase
// ---------------------------------------------------------------------------

class ArchetypeMgr
{
public:
    static ArchetypeMgr* instance();

    void LoadFromDB();
    bool IsLoaded() const { return _loaded; }
    size_t GetCount() const { return _archetypes.size(); }

    const ArchetypeDef*  Get(uint32 archetypeId) const;
    const ArchetypeBeat* GetBeat(uint32 archetypeId, uint8 beatNumber) const;

    // IDs of archetypes eligible for a player at zoneId/level (enabled, level range, zone).
    std::vector<uint32> GetEligible(uint32 zoneId, uint8 level) const;

private:
    ArchetypeMgr() = default;
    bool _loaded = false;
    std::vector<ArchetypeDef> _archetypes;
    std::unordered_map<uint32, size_t> _idIndex;

    static DQPattern        ParsePattern(const std::string& s);
    static DQBeatMechanic   ParseMechanic(const std::string& s);
    static DQBeatTransition ParseTransition(const std::string& s);
};

#define sArchetypeMgr ArchetypeMgr::instance()

// ---------------------------------------------------------------------------
// High-bit encoding — marks a templateId slot as an archetype interaction
// ---------------------------------------------------------------------------

inline uint32 EncodeArchetypeId(uint32 id)       { return id | 0x80000000u; }
inline bool   IsArchetypeId(uint32 templateId)   { return (templateId & 0x80000000u) != 0; }
inline uint32 DecodeArchetypeId(uint32 templateId){ return templateId & 0x7FFFFFFFu; }

#endif // DQ_ARCHETYPE_MGR_H
