/*
 * DQ_PropScript — scriptable GO for activate/gather beats
 *
 * ScriptName: DQ_PropGO
 * Assigned to GO entries 900100-900102 (Herb Bundle, Supply Crate, Fallen Pack).
 *
 * When the player interacts with the GO, OnGossipHello forwards to
 * DynamicQuestMgr::OnPropActivated, which dispatches to the active mechanic.
 * MechanicArchetype::OnActivate increments the beat's objective count and
 * completes the beat when the target is reached.
 */

#include "DynamicQuestMgr.h"
#include "GameObjectScript.h"
#include "Player.h"
#include "ScriptMgr.h"

class DQ_PropScript : public GameObjectScript
{
public:
    DQ_PropScript() : GameObjectScript("DQ_PropGO") {}

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        sDQMgr->OnPropActivated(player, go);
        return true;
    }
};

void AddSC_DQ_PropScript()
{
    new DQ_PropScript();
}
