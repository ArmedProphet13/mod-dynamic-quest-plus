/*
 * DynamicQuests+ Module
 * PlayerScript: hooks OnUpdate, OnCreatureKill, OnLootItem, OnZoneChange, OnLogout, OnLogin
 */

#include "DynamicQuestMgr.h"
#include "Creature.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"

class DQ_PlayerScript : public PlayerScript
{
public:
    DQ_PlayerScript() : PlayerScript("DQ_PlayerScript",
    {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_UPDATE_ZONE,
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_LOOT_ITEM,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_LOGIN,
    }) {}

    void OnPlayerUpdate(Player* player, uint32 p_time) override
    {
        sDQMgr->OnPlayerUpdate(player, p_time);
    }

    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        sDQMgr->OnPlayerZoneChange(player, newZone, newArea);
    }

    void OnPlayerCreatureKill(Player* player, Creature* killed) override
    {
        sDQMgr->OnPlayerKill(player, killed);
    }

    void OnPlayerLootItem(Player* player, Item* item, uint32 /*count*/, ObjectGuid /*lootguid*/) override
    {
        if (item)
            sDQMgr->OnPlayerLoot(player, item->GetEntry());
    }

    void OnPlayerLogout(Player* player) override
    {
        sDQMgr->OnPlayerLogout(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        sDQMgr->OnPlayerLogin(player);
    }
};

void AddSC_DQ_PlayerScript()
{
    new DQ_PlayerScript();
}
