/*
 * DynamicQuests+ Module
 * System 2: Interaction Eligibility Engine — Implementation
 */

#include "EligibilityEngine.h"

EligibilityResult EligibilityEngine::Check(
    const PlayerContext& ctx,
    uint32 cooldownRemainMs)
{
    EligibilityResult result;
    result.passNotAFK      = !ctx.isAFK;
    result.passNotCombat   = !ctx.isInCombat;
    result.passOutdoors    = ctx.isOutdoors;
    result.passNotDungeon  = !ctx.isInDungeon;
    result.passActive      = ctx.hasMovedRecently;
    result.passCooldown    = (cooldownRemainMs == 0);
    result.passNotFlying   = !ctx.isFlying;
    result.passNotSwimming = !ctx.isSwimming;

    result.eligible = result.passNotAFK
        && result.passNotCombat
        && result.passOutdoors
        && result.passNotDungeon
        && result.passActive
        && result.passCooldown
        && result.passNotFlying
        && result.passNotSwimming;

    return result;
}

uint8 EligibilityEngine::GetContextScore(const PlayerContext& ctx)
{
    switch (ctx.activityContext)
    {
        case DQ_ACTIVITY_TRAVELING:  return DQ_CONTEXT_TRAVELING;
        case DQ_ACTIVITY_GRINDING:   return DQ_CONTEXT_GRINDING;
        case DQ_ACTIVITY_EXPLORING:  return DQ_CONTEXT_EXPLORING;
        case DQ_ACTIVITY_IDLE:       return DQ_CONTEXT_IDLE;
        default:                     return DQ_CONTEXT_ANY;
    }
}
