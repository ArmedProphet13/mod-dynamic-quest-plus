/*
 * DynamicQuests+ Module
 * System 2: Interaction Eligibility Engine
 *
 * Reads PlayerContext, answers: should an interaction trigger now?
 * Separate from PCO so trigger policy can change without touching measurement.
 */

#ifndef DQ_ELIGIBILITY_ENGINE_H
#define DQ_ELIGIBILITY_ENGINE_H

#include "PlayerContextObserver.h"
#include <cstdint>

struct EligibilityResult
{
    bool eligible         = false;
    bool passNotAFK       = false;
    bool passNotCombat    = false;
    bool passOutdoors     = false;
    bool passNotDungeon   = false;
    bool passActive       = false;
    bool passCooldown     = false;
    bool passNotFlying    = false; // blocked while airborne on flying mount
    bool passNotSwimming  = false; // blocked while swimming
};

static constexpr uint8 DQ_CONTEXT_ANY       = 0xFF;
static constexpr uint8 DQ_CONTEXT_TRAVELING = (1 << 0);
static constexpr uint8 DQ_CONTEXT_GRINDING  = (1 << 1);
static constexpr uint8 DQ_CONTEXT_IDLE      = (1 << 2);
static constexpr uint8 DQ_CONTEXT_EXPLORING = (1 << 3);

class EligibilityEngine
{
public:
    static EligibilityResult Check(
        const PlayerContext& ctx,
        uint32 cooldownRemainMs);

    static uint8 GetContextScore(const PlayerContext& ctx);
};

#endif // DQ_ELIGIBILITY_ENGINE_H
