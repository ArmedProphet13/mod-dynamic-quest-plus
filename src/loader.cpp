/*
 * DynamicQuests+ Module
 * Script Loader
 *
 * Name must be: Add<module-dir-with-hyphens-as-underscores>Scripts()
 */

void AddSC_DQ_PlayerScript();
void AddSC_DQ_WorldScript();
void AddSC_DQ_CommandScript();
void AddSC_dq_chest_items();
void AddSC_DQ_CourierAI();
void AddSC_DQ_PropScript();

void Addmod_dynamic_quest_plusScripts()
{
    AddSC_DQ_WorldScript();
    AddSC_DQ_PlayerScript();
    AddSC_DQ_CommandScript();
    AddSC_dq_chest_items();
    AddSC_DQ_CourierAI();
    AddSC_DQ_PropScript();
}
