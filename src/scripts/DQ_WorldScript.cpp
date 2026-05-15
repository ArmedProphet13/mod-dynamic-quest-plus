/*
 * DynamicQuests+ Module
 * WorldScript: startup initialization and config reload hooks
 */

#include "DynamicQuestMgr.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "WorldScript.h"

class DQ_WorldScript : public WorldScript
{
public:
    DQ_WorldScript() : WorldScript("DQ_WorldScript",
    {
        WORLDHOOK_ON_STARTUP,
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
    }) {}

    void OnStartup() override
    {
        sDQMgr->Initialize();
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sDQMgr->LoadConfig();
    }
};

void AddSC_DQ_WorldScript()
{
    new DQ_WorldScript();
}
