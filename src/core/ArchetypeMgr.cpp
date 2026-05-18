/*
 * DynamicQuests+ Module
 * System 7: Archetype Engine — LoadFromDB
 */

#include "ArchetypeMgr.h"
#include "DatabaseEnv.h"
#include "DQContextResolver.h"
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
        "SELECT id, name, pattern, total_beats, zone_id, level_min, level_max, enabled, appearance, "
        "required_tags, excluded_tags "
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

        // System 5: parse required_tags / excluded_tags CSV into vectors
        auto parseCsv = [](const std::string& csv, std::vector<std::string>& out)
        {
            if (csv.empty()) return;
            size_t start = 0, end;
            while ((end = csv.find(',', start)) != std::string::npos)
            {
                std::string tok = csv.substr(start, end - start);
                while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
                while (!tok.empty() && tok.back()  == ' ') tok.pop_back();
                if (!tok.empty()) out.push_back(std::move(tok));
                start = end + 1;
            }
            std::string tok = csv.substr(start);
            while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
            while (!tok.empty() && tok.back()  == ' ') tok.pop_back();
            if (!tok.empty()) out.push_back(std::move(tok));
        };
        parseCsv(f[9].Get<std::string>(),  def.requiredTags);
        parseCsv(f[10].Get<std::string>(), def.excludedTags);

        size_t idx = _archetypes.size();
        _idIndex[def.id] = idx;
        _archetypes.push_back(std::move(def));
    } while (result->NextRow());

    QueryResult beats = WorldDatabase.Query(
        "SELECT archetype_id, beat_number, display_id, zone_id, mechanic, "
        "transition_type, transition_value, text_greeting, text_chase, "
        "reward_pool, "
        "spawn_style, prop_entry, prop_count, prop_radius, "
        "emotion, emotion_end, text_on_accept, text_on_complete, "
        "item_prereq_class, item_prereq_subclass, item_consume, "
        "cost_gold_percent, cost_hp_percent, "
        "mechanic_passive, choice_success_transition, choice_fail_transition, "
        "entry_animation, exit_animation, exit_style, "
        "entry_spell, exit_spell, aura_spell, "
        "fight_threshold, cast_school, npc_level_offset "
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
            beat.rewardPool      = f[9].Get<std::string>();
            beat.spawnStyle      = f[10].Get<std::string>();
            beat.propEntry       = f[11].Get<uint32>();
            beat.propCount       = f[12].Get<uint8>();
            beat.propRadius      = f[13].Get<float>();
            beat.emotion         = f[14].Get<std::string>();
            beat.emotionEnd      = f[15].Get<std::string>();
            beat.textOnAccept    = f[16].Get<std::string>();
            beat.textOnComplete  = f[17].Get<std::string>();
            beat.itemPrereqClass    = f[18].Get<uint8>();
            beat.itemPrereqSubclass = f[19].Get<uint8>();
            beat.itemConsume        = f[20].Get<uint8>();
            beat.costGoldPercent    = f[21].Get<uint8>();
            beat.costHpPercent      = f[22].Get<uint8>();
            beat.mechanicPassive           = f[23].Get<uint8>();
            beat.choiceSuccessTransition   = f[24].Get<uint8>();
            beat.choiceFailTransition      = f[25].Get<uint8>();
            beat.entryAnimation            = f[26].Get<std::string>();
            beat.exitAnimation             = f[27].Get<std::string>();
            beat.exitStyle                 = f[28].Get<std::string>();
            beat.entrySpell                = f[29].Get<uint32>();
            beat.exitSpell                 = f[30].Get<uint32>();
            beat.auraSpell                 = f[31].Get<uint32>();
            beat.fightThreshold            = f[32].Get<uint8>();
            beat.castSchool                = f[33].Get<uint8>();
            beat.npcLevelOffset            = f[34].Get<int8>();

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

std::vector<uint32> ArchetypeMgr::GetEligible(uint32 zoneId, uint8 level, const DQTagSet& tags) const
{
    std::vector<uint32> eligible;
    for (const ArchetypeDef& def : _archetypes)
    {
        if (!def.enabled) continue;
        if (def.levelMin > level || def.levelMax < level) continue;
        if (def.zoneId != 0 && def.zoneId != zoneId) continue;
        if (!sDQContext->PassesFilter(tags, def.requiredTags, def.excludedTags)) continue;
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
    if (s == "fight")    return DQ_BEAT_FIGHT;
    if (s == "cast")     return DQ_BEAT_CAST;
    return DQ_BEAT_WITNESS;
}

DQBeatTransition ArchetypeMgr::ParseTransition(const std::string& s)
{
    if (s == "encounter_count") return DQ_TRANS_ENCOUNTER_COUNT;
    if (s == "choice")          return DQ_TRANS_CHOICE;
    if (s == "timer")           return DQ_TRANS_TIMER;
    return DQ_TRANS_QUEST_COMPLETE;
}
