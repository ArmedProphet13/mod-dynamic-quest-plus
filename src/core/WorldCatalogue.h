/*
 * DynamicQuests+ Module
 * System 3: World Catalogue
 *
 * Tagged, classified index of world NPCs viable as DQ+ interaction actors.
 * Built by bootstrap SQL, loaded into memory at startup.
 * All runtime queries are in-memory — zero DB overhead per player tick.
 */

#ifndef DQ_WORLD_CATALOGUE_H
#define DQ_WORLD_CATALOGUE_H

#include "Define.h"
#include <random>
#include <string>
#include <vector>
#include <unordered_map>

struct WCEntry
{
    uint32      entry        = 0;
    uint32      zoneId       = 0;
    uint8       creatureType = 0;
    uint16      faction      = 0;
    uint32      displayId    = 0;
    uint8       threatLevel  = 0;
    bool        excluded     = false;
    std::string tags;         // comma-separated: "demon,humanoid,horde"

    bool HasTag(const std::string& tag) const;
};

class WorldCatalogue
{
public:
    static WorldCatalogue* instance();

    void LoadFromDB();

    std::vector<uint32> GetNPCCandidates(
        uint32 zoneId,
        const std::vector<std::string>& requiredTags,
        const std::vector<std::string>& excludedTags,
        uint8 maxThreatLevel = 1) const;

    std::vector<const WCEntry*> GetZoneEntries(uint32 zoneId) const;

    // Returns the DQ+ courier creature entry for a given template theme.
    // Falls back to 'social' (900004) if no theme match is found.
    uint32 GetCourierEntryForTheme(const std::string& theme) const;

    // Returns a random world-NPC display ID matching tag/zone requirements.
    // Returns 0 if no match is found; caller should keep the default model.
    uint32 GetWorldCourierDisplayId(
        uint32 zoneId,
        const std::vector<std::string>& requiredTags,
        const std::vector<std::string>& excludedTags,
        uint8 maxThreat) const;

    size_t GetEntryCount() const { return _entries.size(); }
    bool IsLoaded() const { return _loaded; }

private:
    WorldCatalogue() = default;
    bool _loaded = false;
    std::vector<WCEntry> _entries;
    std::unordered_map<uint32, std::vector<size_t>> _zoneIndex;
    std::unordered_map<std::string, uint32> _courierByTheme; // theme → courier entry
    mutable std::mt19937 _rng{ std::random_device{}() };

    static bool EntryMatchesTags(
        const WCEntry& e,
        const std::vector<std::string>& required,
        const std::vector<std::string>& excluded,
        uint8 maxThreat);
};

#define sWorldCatalogue WorldCatalogue::instance()

#endif // DQ_WORLD_CATALOGUE_H
