/*
 * DynamicQuests+ Module
 * System 4: Interaction Template Library — Implementation
 */

#include "InteractionTemplateLibrary.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"
#include <sstream>

InteractionTemplateLibrary* InteractionTemplateLibrary::instance()
{
    static InteractionTemplateLibrary instance;
    return &instance;
}

void InteractionTemplateLibrary::LoadFromDB()
{
    _templates.clear();
    _idIndex.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT id, name, theme, npc_tags_req, npc_tags_excl, zone_type, "
        "level_min, level_max, prereq_gold, prereq_item, context_tags, "
        "tier, mechanic_type, rarity_weight, "
        "combat_hp_pct, combat_dmg_pct, gossip_npc_text_id, "
        "combat_hp_ratio, combat_dmg_ratio, zone_faction "
        "FROM dq_interaction_template ORDER BY id");

    if (!result)
    {
        LOG_WARN("module.dynamicquests", "InteractionTemplateLibrary: dq_interaction_template is empty.");
        _loaded = true;
        return;
    }

    do
    {
        Field* f = result->Fetch();
        InteractionTemplate t;
        t.id              = f[0].Get<uint32>();
        t.name            = f[1].Get<std::string>();
        t.theme           = f[2].Get<std::string>();
        t.npcTagsRequired = f[3].Get<std::string>();
        t.npcTagsExcluded = f[4].Get<std::string>();
        t.zoneType        = ParseZoneType(f[5].Get<std::string>());
        t.levelMin        = f[6].Get<uint8>();
        t.levelMax        = f[7].Get<uint8>();
        t.prereqGold      = f[8].Get<uint32>();
        t.prereqItemId    = f[9].Get<uint32>();
        t.contextMask     = ParseContextMask(f[10].Get<std::string>());
        t.tier            = f[11].Get<uint8>();
        t.mechanicType    = ParseMechanicType(f[12].Get<std::string>());
        t.rarityWeight    = f[13].Get<int32>();
        t.combatHpPct     = f[14].Get<int16>();
        t.combatDmgPct    = f[15].Get<int16>();
        t.gossipNpcTextId = f[16].Get<uint32>();
        t.combatHpRatio   = f[17].Get<float>();
        t.combatDmgRatio  = f[18].Get<float>();
        t.zoneFaction     = ParseZoneFaction(f[19].Get<std::string>());
        t.requiredTagList = SplitTags(t.npcTagsRequired);
        t.excludedTagList = SplitTags(t.npcTagsExcluded);

        size_t idx = _templates.size();
        _idIndex[t.id] = idx;
        _templates.push_back(std::move(t));
    } while (result->NextRow());

    QueryResult choices = WorldDatabase.Query(
        "SELECT id, template_id, display_order, choice_text, prereq_item_id, "
        "outcome_type, outcome_data, emote_on_select "
        "FROM dq_interaction_choice ORDER BY template_id, display_order");

    if (choices)
    {
        do
        {
            Field* f = choices->Fetch();
            InteractionChoice c;
            c.id           = f[0].Get<uint32>();
            c.templateId   = f[1].Get<uint32>();
            c.displayOrder = f[2].Get<uint8>();
            c.choiceText   = f[3].Get<std::string>();
            c.prereqItemId = f[4].Get<uint32>();
            c.outcomeType  = ParseOutcomeType(f[5].Get<std::string>());
            c.outcomeData  = f[6].Get<std::string>();
            c.emoteOnSelect = f[7].Get<uint32>();

            auto it = _idIndex.find(c.templateId);
            if (it != _idIndex.end())
                _templates[it->second].choices.push_back(std::move(c));
        } while (choices->NextRow());
    }

    QueryResult variants = WorldDatabase.Query(
        "SELECT template_id, variant_type, text, weight "
        "FROM dq_text_variant WHERE locale='enUS' ORDER BY template_id, variant_type");

    if (variants)
    {
        uint32 variantCount = 0;
        do
        {
            Field* f      = variants->Fetch();
            uint32 tid    = f[0].Get<uint32>();
            std::string vtype = f[1].Get<std::string>();
            std::string text  = f[2].Get<std::string>();
            uint32 weight = f[3].Get<uint32>();

            auto it = _idIndex.find(tid);
            if (it == _idIndex.end()) continue;

            InteractionTemplate& t = _templates[it->second];
            // Push the text once per weight unit so random selection is naturally weighted.
            uint32 reps = (weight == 0) ? 1 : weight;
            for (uint32 i = 0; i < reps; ++i)
            {
                if (vtype == "greeting")
                    t.greetingVariants.push_back(text);
                else if (vtype == "chase")
                    t.chaseVariants.push_back(text);
            }
            ++variantCount;
        } while (variants->NextRow());
        LOG_INFO("module.dynamicquests", "InteractionTemplateLibrary: Loaded {} text variant rows.", variantCount);
    }

    _loaded = true;
    LOG_INFO("module.dynamicquests", "InteractionTemplateLibrary: Loaded {} templates.", _templates.size());
}

const InteractionTemplate* InteractionTemplateLibrary::GetById(uint32 id) const
{
    auto it = _idIndex.find(id);
    if (it == _idIndex.end()) return nullptr;
    return &_templates[it->second];
}

DQZoneFaction InteractionTemplateLibrary::ParseZoneFaction(const std::string& s)
{
    if (s == "alliance") return DQ_FACTION_ALLIANCE;
    if (s == "horde")    return DQ_FACTION_HORDE;
    if (s == "neutral")  return DQ_FACTION_NEUTRAL;
    return DQ_FACTION_ANY;
}

DQZoneType InteractionTemplateLibrary::ParseZoneType(const std::string& s)
{
    if (s == "outdoor")           return DQ_ZONE_OUTDOOR;
    if (s == "capital")           return DQ_ZONE_CAPITAL;
    if (s == "wilderness")        return DQ_ZONE_WILDERNESS;
    if (s == "dungeon_entrance")  return DQ_ZONE_DUNGEON_ENTRANCE;
    return DQ_ZONE_ANY;
}

DQMechanicType InteractionTemplateLibrary::ParseMechanicType(const std::string& s)
{
    if (s == "succubus")         return DQ_MECHANIC_SUCCUBUS;
    if (s == "hungry_child")     return DQ_MECHANIC_HUNGRY_CHILD;
    return DQ_MECHANIC_UNKNOWN;
}

DQOutcomeType InteractionTemplateLibrary::ParseOutcomeType(const std::string& s)
{
    if (s == "reward")         return DQ_OUTCOME_REWARD;
    if (s == "deny")           return DQ_OUTCOME_DENY;
    if (s == "combat")         return DQ_OUTCOME_COMBAT;
    if (s == "fetch")          return DQ_OUTCOME_FETCH;
    if (s == "fail")           return DQ_OUTCOME_FAIL;
    if (s == "phase_advance")  return DQ_OUTCOME_PHASE_ADVANCE;
    return DQ_OUTCOME_DENY;
}

uint8 InteractionTemplateLibrary::ParseContextMask(const std::string& s)
{
    if (s.empty() || s == "any") return 0xFF;
    uint8 mask = 0;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
    {
        if (tok == "traveling") mask |= (1 << 0);
        if (tok == "grinding")  mask |= (1 << 1);
        if (tok == "idle")      mask |= (1 << 2);
        if (tok == "exploring") mask |= (1 << 3);
    }
    return mask ? mask : 0xFF;
}

std::vector<std::string> InteractionTemplateLibrary::SplitTags(const std::string& csv)
{
    std::vector<std::string> result;
    std::stringstream ss(csv);
    std::string tok;
    while (std::getline(ss, tok, ','))
    {
        size_t s = tok.find_first_not_of(' ');
        if (s == std::string::npos) continue;
        size_t e = tok.find_last_not_of(' ');
        result.push_back(tok.substr(s, e - s + 1));
    }
    return result;
}
