/*
 * DynamicQuests+ Module
 * System 7: Archetype Engine — LoadFromDB
 */

#include "ArchetypeMgr.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"

ArchetypeMgr* ArchetypeMgr::instance()
{
    static ArchetypeMgr instance;
    return &instance;
}

void ArchetypeMgr::LoadFromDB()
{
    _archetypes.clear();
    _idIndex.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT id, name, pattern, total_beats, zone_id, level_min, level_max, enabled, appearance "
        "FROM dq_archetype ORDER BY id");

    if (!result)
    {
        LOG_WARN("module.dynamicquests", "ArchetypeMgr: dq_archetype is empty.");
        _loaded = true;
        return;
    }

    do
    {
        Field* f = result->Fetch();
        ArchetypeDef def;
        def.id         = f[0].Get<uint32>();
        def.name       = f[1].Get<std::string>();
        def.pattern    = ParsePattern(f[2].Get<std::string>());
        def.totalBeats = f[3].Get<uint8>();
        def.zoneId     = f[4].Get<uint32>();
        def.levelMin   = f[5].Get<uint8>();
        def.levelMax   = f[6].Get<uint8>();
        def.enabled    = f[7].Get<uint8>() != 0;
        def.appearance = f[8].Get<std::string>();

        size_t idx = _archetypes.size();
        _idIndex[def.id] = idx;
        _archetypes.push_back(std::move(def));
    } while (result->NextRow());

    QueryResult beats = WorldDatabase.Query(
        "SELECT archetype_id, beat_number, display_id, zone_id, mechanic, "
        "transition_type, transition_value, text_greeting, text_chase, "
        "emote_on_arrive, emote_on_complete, reward_pool, "
        "spawn_style, prop_entry, prop_count, prop_radius, "
        "emotion, emotion_end, text_on_accept, text_on_complete "
        "FROM dq_archetype_beat ORDER BY archetype_id, beat_number");

    uint32 beatCount = 0;
    if (beats)
    {
        do
        {
            Field* f = beats->Fetch();
            ArchetypeBeat beat;
            beat.archetypeId     = f[0].Get<uint32>();
            beat.beatNumber      = f[1].Get<uint8>();
            beat.displayId       = f[2].Get<uint32>();
            beat.zoneId          = f[3].Get<uint32>();
            beat.mechanic        = ParseMechanic(f[4].Get<std::string>());
            beat.transitionType  = ParseTransition(f[5].Get<std::string>());
            beat.transitionValue = f[6].Get<uint16>();
            beat.textGreeting    = f[7].Get<std::string>();
            beat.textChase       = f[8].Get<std::string>();
            beat.emoteOnArrive   = f[9].Get<int16>();
            beat.emoteOnComplete = f[10].Get<int16>();
            beat.rewardPool      = f[11].Get<std::string>();
            beat.spawnStyle      = f[12].Get<std::string>();
            beat.propEntry       = f[13].Get<uint32>();
            beat.propCount       = f[14].Get<uint8>();
            beat.propRadius      = f[15].Get<float>();
            beat.emotion         = f[16].Get<std::string>();
            beat.emotionEnd      = f[17].Get<std::string>();
            beat.textOnAccept    = f[18].Get<std::string>();
            beat.textOnComplete  = f[19].Get<std::string>();

            auto it = _idIndex.find(beat.archetypeId);
            if (it != _idIndex.end())
                _archetypes[it->second].beats.push_back(std::move(beat));
            ++beatCount;
        } while (beats->NextRow());
    }

    _loaded = true;
    LOG_INFO("module.dynamicquests", "ArchetypeMgr: Loaded {} archetypes, {} beats.",
        _archetypes.size(), beatCount);
}

const ArchetypeDef* ArchetypeMgr::Get(uint32 archetypeId) const
{
    auto it = _idIndex.find(archetypeId);
    if (it == _idIndex.end()) return nullptr;
    return &_archetypes[it->second];
}

const ArchetypeBeat* ArchetypeMgr::GetBeat(uint32 archetypeId, uint8 beatNumber) const
{
    const ArchetypeDef* def = Get(archetypeId);
    if (!def) return nullptr;
    for (const ArchetypeBeat& b : def->beats)
        if (b.beatNumber == beatNumber)
            return &b;
    return nullptr;
}

std::vector<uint32> ArchetypeMgr::GetEligible(uint32 zoneId, uint8 level) const
{
    std::vector<uint32> eligible;
    for (const ArchetypeDef& def : _archetypes)
    {
        if (!def.enabled) continue;
        if (def.levelMin > level || def.levelMax < level) continue;
        if (def.zoneId != 0 && def.zoneId != zoneId) continue;
        eligible.push_back(def.id);
    }
    return eligible;
}

DQPattern ArchetypeMgr::ParsePattern(const std::string& s)
{
    if (s == "counter")   return DQ_PATTERN_COUNTER;
    if (s == "branching") return DQ_PATTERN_BRANCHING;
    return DQ_PATTERN_SEQUENTIAL;
}

DQBeatMechanic ArchetypeMgr::ParseMechanic(const std::string& s)
{
    if (s == "courier")  return DQ_BEAT_COURIER;
    if (s == "goto")     return DQ_BEAT_GOTO;
    if (s == "kill")     return DQ_BEAT_KILL;
    if (s == "activate") return DQ_BEAT_ACTIVATE;
    return DQ_BEAT_WITNESS;
}

DQBeatTransition ArchetypeMgr::ParseTransition(const std::string& s)
{
    if (s == "encounter_count") return DQ_TRANS_ENCOUNTER_COUNT;
    if (s == "choice")          return DQ_TRANS_CHOICE;
    if (s == "timer")           return DQ_TRANS_TIMER;
    return DQ_TRANS_QUEST_COMPLETE;
}
