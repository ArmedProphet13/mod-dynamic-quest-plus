/*
 * DynamicQuests+ Module
 * NPC Matching — CourierSelection result type
 *
 * FindBestCourier was removed with the legacy InteractionTemplate system (v7).
 * CourierSelection is retained as the return type for archetype spawn paths.
 */

#ifndef DQ_NPC_MATCHING_ENGINE_H
#define DQ_NPC_MATCHING_ENGINE_H

#include "Define.h"

struct CourierSelection
{
    uint32 entry     = 0;  // dedicated AI entry (900001-900005)
    uint32 displayId = 0;  // world NPC display override (0 = keep default model)
};

#endif // DQ_NPC_MATCHING_ENGINE_H
