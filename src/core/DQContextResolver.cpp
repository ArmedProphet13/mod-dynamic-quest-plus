/*
 * DynamicQuests+ Module
 * System 5: Position & Context Resolver — implementation
 */

#include "DQContextResolver.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Field.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "QueryResult.h"
#include "SharedDefines.h"
#include <algorithm>
#include <cmath>
#include <sstream>

static constexpr float GRAVEYARD_RANGE = 150.f;

DQContextResolver* DQContextResolver::instance()
{
    static DQContextResolver inst;
    return &inst;
}

// ---------------------------------------------------------------------------
// Initialize — load all three DB tables
// ---------------------------------------------------------------------------

void DQContextResolver::Initialize()
{
    _areaTags.clear();
    _parents.clear();
    _graveyards.clear();
    _landmarks.clear();

    // dq_area_tags
    {
        QueryResult r = WorldDatabase.Query("SELECT area_id, tag FROM dq_area_tags");
        uint32 count = 0;
        if (r)
            do
            {
                Field* f = r->Fetch();
                _areaTags[f[0].Get<uint32>()].push_back(f[1].Get<std::string>());
                ++count;
            } while (r->NextRow());
        LOG_INFO("module.dynamicquests", "DQContextResolver: {} area tags loaded.", count);
    }

    // dq_tag_hierarchy
    {
        QueryResult r = WorldDatabase.Query("SELECT child_tag, parent_tag FROM dq_tag_hierarchy");
        uint32 count = 0;
        if (r)
            do
            {
                Field* f = r->Fetch();
                _parents[f[0].Get<std::string>()].push_back(f[1].Get<std::string>());
                ++count;
            } while (r->NextRow());
        LOG_INFO("module.dynamicquests", "DQContextResolver: {} tag hierarchy entries loaded.", count);
    }

    // dq_landmarks
    {
        QueryResult r = WorldDatabase.Query(
            "SELECT map_id, x, y, z, tag, radius FROM dq_landmarks");
        uint32 count = 0;
        if (r)
            do
            {
                Field* f = r->Fetch();
                Landmark lm;
                lm.mapId  = f[0].Get<uint32>();
                lm.x      = f[1].Get<float>();
                lm.y      = f[2].Get<float>();
                lm.z      = f[3].Get<float>();
                lm.tag    = f[4].Get<std::string>();
                lm.radius = f[5].Get<float>();
                _landmarks.push_back(std::move(lm));
                ++count;
            } while (r->NextRow());
        LOG_INFO("module.dynamicquests", "DQContextResolver: {} landmarks loaded.", count);
    }

    // game_graveyard — cache positions for proximity checks
    {
        QueryResult r = WorldDatabase.Query("SELECT map, x, y, z FROM game_graveyard");
        uint32 count = 0;
        if (r)
            do
            {
                Field* f = r->Fetch();
                uint32 mapId = f[0].Get<uint32>();
                GraveyardPos pos;
                pos.x = f[1].Get<float>();
                pos.y = f[2].Get<float>();
                pos.z = f[3].Get<float>();
                _graveyards[mapId].push_back(pos);
                ++count;
            } while (r->NextRow());
        LOG_INFO("module.dynamicquests", "DQContextResolver: {} graveyard positions cached.", count);
    }
}

// ---------------------------------------------------------------------------
// Resolve — 5-step tag resolution
// ---------------------------------------------------------------------------

DQTagSet DQContextResolver::Resolve(Player* player, const PlayerContext& ctx) const
{
    DQTagSet tags;

    // ------------------------------------------------------------------
    // Step 1: Player property tags
    // ------------------------------------------------------------------

    // Class
    static const char* classTags[] = {
        nullptr,          // 0 (unused)
        "warrior",        // 1
        "paladin",        // 2
        "hunter",         // 3
        "rogue",          // 4
        "priest",         // 5
        "deathknight",    // 6
        "shaman",         // 7
        "mage",           // 8
        "warlock",        // 9
        nullptr,          // 10 (unused)
        "druid",          // 11
    };
    uint8 cls = player->getClass();
    if (cls < std::extent<decltype(classTags)>::value && classTags[cls])
        tags.insert(classTags[cls]);

    // Race
    static const char* raceTags[] = {
        nullptr,          // 0 (unused)
        "human",          // 1
        "orc",            // 2
        "dwarf",          // 3
        "night_elf",      // 4
        "undead",         // 5
        "tauren",         // 6
        "gnome",          // 7
        "troll",          // 8
        nullptr,          // 9 (unused)
        "blood_elf",      // 10
        "draenei",        // 11
    };
    uint8 race = ctx.race;
    if (race < std::extent<decltype(raceTags)>::value && raceTags[race])
        tags.insert(raceTags[race]);

    // Faction
    tags.insert(player->GetTeamId() == TEAM_HORDE ? "horde" : "alliance");

    // Group state
    tags.insert(player->GetGroup() ? "grouped" : "solo");

    // Combat / mounted
    if (ctx.isInCombat) tags.insert("in_combat");
    if (player->IsMounted()) tags.insert("mounted");

    // ------------------------------------------------------------------
    // Step 2: Map / area flags (DBC)
    // ------------------------------------------------------------------

    if (ctx.isOutdoors)
        tags.insert("outdoor");
    else
        tags.insert("indoor");

    if (ctx.isSwimming)
        tags.insert("in_water");

    // Instance type — read from map so we can distinguish dungeon vs raid vs bg
    if (Map* map = player->GetMap())
    {
        if (map->IsDungeon())          tags.insert("dungeon");
        if (map->IsRaid())             tags.insert("raid");
        if (map->IsBattleground())     tags.insert("battleground");
        if (map->IsBattleArena())      tags.insert("arena");
    }

    // Area DBC flags
    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(ctx.areaId))
    {
        uint32 flags = area->flags;
        if (flags & (AREA_FLAG_CAPITAL | AREA_FLAG_SLAVE_CAPITAL))
            tags.insert("city");
        if (flags & AREA_FLAG_TOWN)
            tags.insert("town");
        if (flags & AREA_FLAG_SANCTUARY)
            tags.insert("sanctuary");
        if (flags & AREA_FLAG_OUTDOOR_PVP)
            tags.insert("pvp_zone");
    }

    // ------------------------------------------------------------------
    // Step 3: Custom geographic tags from dq_area_tags
    // ------------------------------------------------------------------
    {
        auto it = _areaTags.find(ctx.areaId);
        if (it != _areaTags.end())
            for (const std::string& t : it->second)
                tags.insert(t);
    }

    // ------------------------------------------------------------------
    // Step 4: Proximity tags
    // ------------------------------------------------------------------
    AddProximityTags(player, tags);

    // ------------------------------------------------------------------
    // Step 5: Hierarchy expansion — add parent tags recursively
    // ------------------------------------------------------------------
    ExpandHierarchy(tags);

    return tags;
}

// ---------------------------------------------------------------------------
// AddProximityTags — graveyard + landmark distance checks
// ---------------------------------------------------------------------------

void DQContextResolver::AddProximityTags(Player* player, DQTagSet& tags) const
{
    float px = player->GetPositionX();
    float py = player->GetPositionY();
    float pz = player->GetPositionZ();
    uint32 mapId = player->GetMapId();

    // Graveyard check
    auto it = _graveyards.find(mapId);
    if (it != _graveyards.end())
    {
        for (const GraveyardPos& g : it->second)
        {
            float dx = g.x - px, dy = g.y - py;
            if (std::sqrt(dx*dx + dy*dy) <= GRAVEYARD_RANGE)
            {
                tags.insert("graveyard_nearby");
                break; // one match is enough
            }
        }
    }

    // Landmark checks
    for (const Landmark& lm : _landmarks)
    {
        if (lm.mapId != mapId) continue;
        float dx = lm.x - px, dy = lm.y - py, dz = lm.z - pz;
        if (std::sqrt(dx*dx + dy*dy + dz*dz) <= lm.radius)
            tags.insert(lm.tag);
    }
}

// ---------------------------------------------------------------------------
// ExpandHierarchy — walk _parents, add all ancestor tags
// ---------------------------------------------------------------------------

void DQContextResolver::ExpandHierarchy(DQTagSet& tags) const
{
    // Iterative BFS — avoids stack overflow on deep hierarchies
    std::vector<std::string> queue(tags.begin(), tags.end());
    size_t head = 0;
    while (head < queue.size())
    {
        const std::string& tag = queue[head++];
        auto it = _parents.find(tag);
        if (it == _parents.end()) continue;
        for (const std::string& parent : it->second)
        {
            if (tags.insert(parent).second) // only process newly inserted parents
                queue.push_back(parent);
        }
    }
}

// ---------------------------------------------------------------------------
// PassesFilter
// ---------------------------------------------------------------------------

bool DQContextResolver::PassesFilter(const DQTagSet& tags,
    const std::vector<std::string>& required,
    const std::vector<std::string>& excluded) const
{
    for (const std::string& t : required)
        if (tags.find(t) == tags.end())
            return false;

    for (const std::string& t : excluded)
        if (tags.find(t) != tags.end())
            return false;

    return true;
}

// ---------------------------------------------------------------------------
// DebugDump
// ---------------------------------------------------------------------------

std::string DQContextResolver::DebugDump(const DQTagSet& tags) const
{
    std::vector<std::string> sorted(tags.begin(), tags.end());
    std::sort(sorted.begin(), sorted.end());

    std::ostringstream ss;
    ss << "DQ+ context tags (" << sorted.size() << "): ";
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        if (i) ss << ", ";
        ss << sorted[i];
    }
    return ss.str();
}
