/*
 * DynamicQuests+ Module
 * System 5: NPC Matching Engine
 *
 * Given an interaction template's NPC requirements and the player's PlayerContext,
 * queries the WorldCatalogue and returns the best-fit courier selection.
 * The entry is always a dedicated DQ_CourierAI entry (900001-900005) to ensure
 * correct AI. The displayId override comes from a zone-contextual world NPC so
 * the courier looks like it belongs in the current area.
 */

#ifndef DQ_NPC_MATCHING_ENGINE_H
#define DQ_NPC_MATCHING_ENGINE_H

#include "PlayerContextObserver.h"
#include "InteractionTemplateLibrary.h"
#include <cstdint>

struct CourierSelection
{
    uint32 entry     = 0;  // dedicated AI entry (900001-900005)
    uint32 displayId = 0;  // world NPC display override (0 = keep default model)
};

class NPCMatchingEngine
{
public:
    // Returns the AI entry + display override for the given template + context.
    // entry is always a dedicated DQ+ courier; displayId varies by zone.
    static CourierSelection FindBestCourier(
        const InteractionTemplate& tmpl,
        const PlayerContext& ctx);

private:
    static uint32 PickRandom(const std::vector<uint32>& candidates);
};

#endif // DQ_NPC_MATCHING_ENGINE_H
