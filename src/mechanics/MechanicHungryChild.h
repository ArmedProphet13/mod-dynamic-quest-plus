/*
 * DynamicQuests+ Module
 * Mechanic: HungryChild
 *
 * Intentionally thin. All encounter logic lives in DQ_HungryChildAI:
 * approach walk, sit+cry emotes, any-food gossip, eat/laugh departure.
 * This mechanic stub handles only cleanup on abort/logout.
 */

#ifndef DQ_MECHANIC_HUNGRY_CHILD_H
#define DQ_MECHANIC_HUNGRY_CHILD_H

#include "IMechanicModule.h"

class Player;

class MechanicHungryChild : public IMechanicModule
{
public:
    void OnStart(Player* player, Creature* courier, InteractionInstance& inst) override;
    void OnChoice(Player* player, uint32 choiceId, InteractionInstance& inst) override;
    void OnComplete(Player* player, InteractionInstance& inst) override;
    void OnFail(Player* player, InteractionInstance& inst) override;
    void OnCleanup(Player* player, InteractionInstance& inst) override;

    const char* GetName() const override { return "MechanicHungryChild"; }

    // Returns a faction-appropriate humanoid child display ID for the player's race.
    // Returns 0 if no valid child model exists (quest should be skipped).
    // Alliance: Human 338, others fall back to 338.
    // Horde: Orc 14589, Blood Elf 19315, others fall back to 14589.
    // Never returns a cross-faction or beast-looking model.
    static uint32 GetChildDisplayId(Player* player);
};

#endif // DQ_MECHANIC_HUNGRY_CHILD_H
