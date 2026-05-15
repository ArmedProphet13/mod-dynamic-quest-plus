/*
 * DynamicQuests+ Module
 * System 5: NPC Matching Engine — Implementation
 */

#include "NPCMatchingEngine.h"
#include "WorldCatalogue.h"
#include "Log.h"
#include <random>
#include <chrono>

CourierSelection NPCMatchingEngine::FindBestCourier(
    const InteractionTemplate& tmpl,
    const PlayerContext& ctx)
{
    CourierSelection sel;

    if (!sWorldCatalogue->IsLoaded())
        return sel;

    // AI carrier: always a dedicated DQ_CourierAI entry, chosen by theme.
    sel.entry = sWorldCatalogue->GetCourierEntryForTheme(tmpl.theme);

    // Display override: pick a contextual world NPC display from this zone.
    // Succubus mechanic uses a fixed creature_template_model — skip the override
    // so SpawnCourier leaves the native display intact.
    if (tmpl.mechanicType != DQ_MECHANIC_SUCCUBUS)
    {
        std::vector<std::string> excluded = tmpl.excludedTagList;
        excluded.emplace_back("boss");
        excluded.emplace_back("named_lore");

        sel.displayId = sWorldCatalogue->GetWorldCourierDisplayId(
            ctx.zoneId, tmpl.requiredTagList, excluded, 1);
    }

    LOG_DEBUG("module.dynamicquests.nme",
        "NPCMatchingEngine: template='{}' theme='{}' zone={} -> entry={} displayId={}",
        tmpl.name, tmpl.theme, ctx.zoneId, sel.entry, sel.displayId);

    return sel;
}

uint32 NPCMatchingEngine::PickRandom(const std::vector<uint32>& candidates)
{
    if (candidates.empty())
        return 0;
    if (candidates.size() == 1)
        return candidates[0];

    static std::mt19937 rng{ static_cast<uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()) };
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    return candidates[dist(rng)];
}
