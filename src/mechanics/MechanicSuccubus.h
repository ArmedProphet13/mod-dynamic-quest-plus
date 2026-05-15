/*
 * DynamicQuests+ Module
 * Mechanic: Succubus — Ileana encounter (episodes 1-3)
 *
 * This mechanic is intentionally thin. The full encounter sequence
 * (phases, stun, animation, costs, reward timing) lives in DQ_SuccubusAI.
 * The mechanic handles: choice routing, kill-path rewards, and episode advance.
 */

#ifndef DQ_MECHANIC_SUCCUBUS_H
#define DQ_MECHANIC_SUCCUBUS_H

#include "IMechanicModule.h"

class MechanicSuccubus : public IMechanicModule
{
public:
    void OnStart(Player* player, Creature* courier, InteractionInstance& inst) override;
    void OnChoice(Player* player, uint32 choiceId, InteractionInstance& inst) override;
    void OnComplete(Player* player, InteractionInstance& inst) override;
    void OnFail(Player* player, InteractionInstance& inst) override;
    void OnCleanup(Player* player, InteractionInstance& inst) override;
    void OnKill(Player* player, Creature* victim, InteractionInstance& inst) override;

    const char* GetName() const override { return "MechanicSuccubus"; }

    // Deliver accept-path reward for the given episode (1-3).
    // Called directly by DQ_SuccubusAI after the animation sequence completes.
    static void DeliverAcceptReward(Player* player, uint8 episode);
};

#endif // DQ_MECHANIC_SUCCUBUS_H
