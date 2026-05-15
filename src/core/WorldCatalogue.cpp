/*
 * DynamicQuests+ Module
 * System 3: World Catalogue — Implementation
 */

#include "WorldCatalogue.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"

WorldCatalogue* WorldCatalogue::instance()
{
    static WorldCatalogue instance;
    return &instance;
}

bool WCEntry::HasTag(const std::string& tag) const
{
    size_t pos = tags.find(tag);
    while (pos != std::string::npos)
    {
        bool startOk = (pos == 0 || tags[pos - 1] == ',');
        bool endOk   = (pos + tag.size() == tags.size() || tags[pos + tag.size()] == ',');
        if (startOk && endOk)
            return true;
        pos = tags.find(tag, pos + 1);
    }
    return false;
}

void WorldCatalogue::LoadFromDB()
{
    _entries.clear();
    _zoneIndex.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT c.entry, c.zone_id, c.creature_type, c.faction, c.tags, "
        "c.threat_level, c.excluded, ctm.CreatureDisplayID "
        "FROM dq_world_npc_catalogue c "
        "LEFT JOIN creature_template_model ctm ON ctm.CreatureID = c.entry AND ctm.Idx = 0 "
        "ORDER BY c.zone_id, c.entry");

    if (!result)
    {
        LOG_WARN("module.dynamicquests.wc",
            "WorldCatalogue: dq_world_npc_catalogue is empty. Run .dq bootstrap first.");
    }

    uint32 count = 0;
    if (result) do
    {
        Field* fields = result->Fetch();
        WCEntry e;
        e.entry        = fields[0].Get<uint32>();
        e.zoneId       = fields[1].Get<uint32>();
        e.creatureType = fields[2].Get<uint8>();
        e.faction      = fields[3].Get<uint16>();
        e.tags         = fields[4].Get<std::string>();
        e.threatLevel  = fields[5].Get<uint8>();
        e.excluded     = fields[6].Get<bool>();
        e.displayId    = fields[7].Get<uint32>();

        size_t idx = _entries.size();
        _entries.push_back(std::move(e));
        _zoneIndex[_entries[idx].zoneId].push_back(idx);
        ++count;
    } while (result && result->NextRow());

    _loaded = true;
    LOG_INFO("module.dynamicquests.wc", "WorldCatalogue: Loaded {} NPC entries across {} zones.",
        count, _zoneIndex.size());

    // Load courier theme → entry mapping from dq_courier_template
    _courierByTheme.clear();
    QueryResult couriers = WorldDatabase.Query(
        "SELECT theme_tag, entry FROM dq_courier_template");
    if (couriers)
    {
        do
        {
            Field* f = couriers->Fetch();
            _courierByTheme[f[0].Get<std::string>()] = f[1].Get<uint32>();
        } while (couriers->NextRow());
    }
    LOG_INFO("module.dynamicquests.wc", "WorldCatalogue: Loaded {} courier theme mappings.",
        _courierByTheme.size());
}

bool WorldCatalogue::EntryMatchesTags(
    const WCEntry& e,
    const std::vector<std::string>& required,
    const std::vector<std::string>& excluded,
    uint8 maxThreat)
{
    if (e.excluded)                return false;
    if (e.threatLevel > maxThreat) return false;
    for (const auto& tag : required)
        if (!e.HasTag(tag)) return false;
    for (const auto& tag : excluded)
        if (e.HasTag(tag)) return false;
    return true;
}

std::vector<uint32> WorldCatalogue::GetNPCCandidates(
    uint32 zoneId,
    const std::vector<std::string>& requiredTags,
    const std::vector<std::string>& excludedTags,
    uint8 maxThreatLevel) const
{
    std::vector<uint32> candidates;

    auto addFromZone = [&](uint32 zone)
    {
        auto it = _zoneIndex.find(zone);
        if (it == _zoneIndex.end()) return;
        for (size_t idx : it->second)
        {
            const WCEntry& e = _entries[idx];
            if (EntryMatchesTags(e, requiredTags, excludedTags, maxThreatLevel))
                candidates.push_back(e.entry);
        }
    };

    if (zoneId != 0)
    {
        addFromZone(zoneId);
        if (candidates.empty())
            addFromZone(0); // global pool fallback
    }
    else
    {
        for (const auto& e : _entries)
            if (EntryMatchesTags(e, requiredTags, excludedTags, maxThreatLevel))
                candidates.push_back(e.entry);
    }

    LOG_DEBUG("module.dynamicquests.wc", "GetNPCCandidates zone={} found={}", zoneId, candidates.size());
    return candidates;
}

uint32 WorldCatalogue::GetCourierEntryForTheme(const std::string& theme) const
{
    auto it = _courierByTheme.find(theme);
    if (it != _courierByTheme.end())
        return it->second;

    // Fallback: generic social courier
    auto fallback = _courierByTheme.find("social");
    return (fallback != _courierByTheme.end()) ? fallback->second : 0;
}

uint32 WorldCatalogue::GetWorldCourierDisplayId(
    uint32 zoneId,
    const std::vector<std::string>& requiredTags,
    const std::vector<std::string>& excludedTags,
    uint8 maxThreat) const
{
    // Collect matching candidates: zone-specific first, then global pool, then relaxed tags.
    std::vector<uint32> displayIds;

    auto collect = [&](uint32 zone, const std::vector<std::string>& required)
    {
        auto it = _zoneIndex.find(zone);
        if (it == _zoneIndex.end()) return;
        for (size_t idx : it->second)
        {
            const WCEntry& e = _entries[idx];
            if (e.displayId == 0) continue;
            if (EntryMatchesTags(e, required, excludedTags, maxThreat))
                displayIds.push_back(e.displayId);
        }
    };

    collect(zoneId, requiredTags);
    if (displayIds.empty()) collect(0, requiredTags);

    // Relax to first required tag if still empty
    if (displayIds.empty() && !requiredTags.empty())
    {
        std::vector<std::string> relaxed = { requiredTags[0] };
        collect(zoneId, relaxed);
        if (displayIds.empty()) collect(0, relaxed);
    }

    if (displayIds.empty())
        return 0;

    std::uniform_int_distribution<size_t> dist(0, displayIds.size() - 1);
    return displayIds[dist(_rng)];
}

std::vector<const WCEntry*> WorldCatalogue::GetZoneEntries(uint32 zoneId) const
{
    std::vector<const WCEntry*> result;
    auto it = _zoneIndex.find(zoneId);
    if (it == _zoneIndex.end()) return result;
    for (size_t idx : it->second)
        result.push_back(&_entries[idx]);
    return result;
}
