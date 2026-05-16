/*
 * DynamicQuests+ Module
 * Mechanic Module Interface
 */

#ifndef DQ_IMECHANIC_MODULE_H
#define DQ_IMECHANIC_MODULE_H

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <vector>

class Player;
class Creature;
class GameObject;

struct InteractionInstance
{
    uint32      templateId      = 0;
    uint32      activeQuestId   = 0;
    uint32      choiceIndex     = 0;
    ObjectGuid  courierGuid     = ObjectGuid::Empty;
    ObjectGuid  destNpcGuid     = ObjectGuid::Empty;
    uint32      phaseTimer      = 0;
    uint8       currentPhase    = 0;
    bool        completed       = false;
    bool        failed          = false;

    // Objective tracking (Kill, Collect, Fetch)
    uint32      objectiveCount  = 0;
    uint32      objectiveTarget = 0;
    uint32      objectiveItemId = 0;  // item to collect/fetch
    uint32      objectiveEntry  = 0;  // creature entry to kill (0 = any)
    std::string rewardPool;           // dq_reward_pool pool_name

    // Auxiliary spawns (e.g. herb cluster creatures for collect quests)
    std::vector<ObjectGuid> auxGuidList;

    // Private phase bit assigned to this interaction's courier.
    // Non-zero: all mechanic-spawned creatures should be set to this phase.
    uint32 phaseBit = 0;

    // Fight handler: HP% below which a hostile NPC concedes (from beat.fightThreshold).
    uint8  concedePct = 15;

    // Animation cleanup: persistent aura spell to remove before despawn (from beat.auraSpell).
    uint32 auraSpell = 0;
};

class IMechanicModule
{
public:
    virtual ~IMechanicModule() = default;

    // Called when the courier arrives and the interaction is presented.
    virtual void OnStart(Player* player, Creature* courier, InteractionInstance& inst) = 0;

    // Called when the player selects a gossip choice.
    virtual void OnChoice(Player* player, uint32 choiceId, InteractionInstance& inst) = 0;

    // Called by the mechanic itself when objectives are complete.
    virtual void OnComplete(Player* player, InteractionInstance& inst) = 0;

    // Called when the interaction fails (timeout, death, abort).
    virtual void OnFail(Player* player, InteractionInstance& inst) = 0;

    // Called on .dq stop / logout — must clean up all spawns.
    virtual void OnCleanup(Player* player, InteractionInstance& inst) = 0;

    // Tick while interaction is DQ_STATE_ON_QUEST.
    virtual void OnUpdate(Player* /*player*/, uint32 /*diff*/, InteractionInstance& /*inst*/) {}

    // Called when the player kills a creature while ON_QUEST.
    virtual void OnKill(Player* /*player*/, Creature* /*victim*/, InteractionInstance& /*inst*/) {}

    // Called when the player loots an item while ON_QUEST.
    virtual void OnLoot(Player* /*player*/, uint32 /*itemId*/, InteractionInstance& /*inst*/) {}

    // Called when the player interacts with a prop GO spawned for this beat.
    virtual void OnActivate(Player* /*player*/, GameObject* /*go*/, InteractionInstance& /*inst*/) {}

    // Called when the player interacts with the destination NPC (for fetch/deliver outcomes).
    virtual void OnDelivery(Player* /*player*/, Creature* /*destNpc*/, InteractionInstance& /*inst*/) {}

    // Called by .dq episode skip — GM command to force-advance to next phase.
    virtual void ForceAdvance(Player* /*player*/, InteractionInstance& /*inst*/) {}

    virtual const char* GetName() const = 0;
};

#endif // DQ_IMECHANIC_MODULE_H
