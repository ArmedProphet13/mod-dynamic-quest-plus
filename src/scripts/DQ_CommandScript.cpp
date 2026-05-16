/*
 * DynamicQuests+ Module
 * CommandScript: Full .dq GM command tree
 *
 * All commands require RBAC_PERM_COMMAND_DEBUG level.
 * Multi-level table follows cs_debug.cpp pattern exactly.
 */

#include "ArchetypeMgr.h"
#include "Chat.h"
#include "CommandScript.h"
#include "DQContextResolver.h"
#include "DynamicQuestMgr.h"
#include "InteractionTemplateLibrary.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "WorldCatalogue.h"

using namespace Acore::ChatCommands;

class DQ_CommandScript : public CommandScript
{
public:
    DQ_CommandScript() : CommandScript("DQ_CommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        // .dq debug on/off/gate
        static ChatCommandTable dqDebugTable =
        {
            { "on",   HandleDQDebugOnCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "off",  HandleDQDebugOffCommand,  rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "gate", HandleDQDebugGateCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
        };

        // .dq trigger  /  .dq trigger tier1  /  .dq trigger episode  /  .dq trigger archetype
        // Empty-string entry = handler when no subcommand is given (.dq trigger [player])
        static ChatCommandTable dqTriggerTable =
        {
            { "",          HandleDQTriggerCommand,          rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "tier1",     HandleDQTriggerTier1Command,     rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "episode",   HandleDQTriggerEpisodeCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "archetype", HandleDQTriggerArchetypeCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
        };

        // .dq history  /  .dq history clear
        static ChatCommandTable dqHistoryTable =
        {
            { "",      HandleDQHistoryCommand,      rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "clear", HandleDQHistoryClearCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
        };

        // .dq episode list/skip/fail/spawn
        static ChatCommandTable dqEpisodeTable =
        {
            { "list",  HandleDQEpisodeListCommand,  rbac::RBAC_PERM_COMMAND_DEBUG, Console::Yes },
            { "skip",  HandleDQEpisodeSkipCommand,  rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "fail",  HandleDQEpisodeFailCommand,  rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "spawn", HandleDQEpisodeSpawnCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
        };

        // .dq catalogue zone / query
        static ChatCommandTable dqCatalogueTable =
        {
            { "zone",  HandleDQCatalogueZoneCommand,  rbac::RBAC_PERM_COMMAND_DEBUG, Console::Yes },
            { "query", HandleDQCatalogueQueryCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::Yes },
        };

        // .dq root — no duplicate keys
        static ChatCommandTable dqCommandTable =
        {
            { "trigger",   dqTriggerTable },
            { "stop",      HandleDQStopCommand,      rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "status",    HandleDQStatusCommand,    rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "context",   HandleDQContextCommand,   rbac::RBAC_PERM_COMMAND_DEBUG, Console::No  },
            { "history",   dqHistoryTable },
            { "debug",     dqDebugTable },
            { "episode",   dqEpisodeTable },
            { "catalogue", dqCatalogueTable },
            { "reload",    HandleDQReloadCommand,    rbac::RBAC_PERM_COMMAND_DEBUG, Console::Yes },
            { "bootstrap", HandleDQBootstrapCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "dq", dqCommandTable },
        };

        return commandTable;
    }

    // .dq trigger [player]
    static bool HandleDQTriggerCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->ForceTriggger(target);
        handler->PSendSysMessage("DQ+ courier triggered for {}.", target->GetName());
        return true;
    }

    // .dq trigger tier1 [player]
    static bool HandleDQTriggerTier1Command(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        for (const auto& tmpl : sInteractionLib->GetAll())
        {
            if (tmpl.tier == 1 && tmpl.levelMin <= target->GetLevel())
            {
                sDQMgr->ForceTriggger(target, tmpl.id);
                handler->PSendSysMessage("DQ+ tier1 courier triggered (template={}) for {}.",
                    tmpl.id, target->GetName());
                return true;
            }
        }
        handler->SendErrorMessage("No tier 1 template found for level {}.", target->GetLevel());
        return false;
    }

    // .dq trigger episode #id [player]
    static bool HandleDQTriggerEpisodeCommand(ChatHandler* handler,
        uint32 templateId, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->ForceTriggger(target, templateId);
        handler->PSendSysMessage("DQ+ episode {} triggered for {}.", templateId, target->GetName());
        return true;
    }

    // .dq trigger archetype #id [player]  — bypasses zone check, useful for testing
    static bool HandleDQTriggerArchetypeCommand(ChatHandler* handler,
        uint32 archetypeId, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->ForceTriggger(target, EncodeArchetypeId(archetypeId));
        handler->PSendSysMessage("DQ+ archetype {} triggered for {}.", archetypeId, target->GetName());
        return true;
    }

    // .dq stop [player]
    static bool HandleDQStopCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->AbortInteraction(target);
        handler->PSendSysMessage("DQ+ interaction aborted for {}.", target->GetName());
        return true;
    }

    // .dq context [player] — print resolved tag set and archetype PASS/FAIL
    static bool HandleDQContextCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        PlayerContext ctx;
        ctx.zoneId     = target->GetZoneId();
        ctx.areaId     = target->GetAreaId();
        ctx.mapId      = target->GetMapId();
        ctx.level      = target->GetLevel();
        ctx.race       = target->getRace();
        ctx.isOutdoors = target->IsOutdoors();
        ctx.isInCombat = target->IsInCombat();
        ctx.isSwimming = target->IsInWater();

        DQTagSet tags = sDQContext->Resolve(target, ctx);
        handler->SendSysMessage(sDQContext->DebugDump(tags).c_str());

        auto eligible = sArchetypeMgr->GetEligible(ctx.zoneId, ctx.level, tags);
        handler->PSendSysMessage("Archetypes eligible: {}/{}", eligible.size(), sArchetypeMgr->GetCount());
        for (uint32 arcId : eligible)
        {
            const ArchetypeDef* def = sArchetypeMgr->Get(arcId);
            if (def)
                handler->PSendSysMessage("  PASS [{}] {}", def->id, def->name);
        }
        return true;
    }

    // .dq status [player]
    static bool HandleDQStatusCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        std::string status = sDQMgr->GetStatusString(target);
        handler->SendSysMessage(status.c_str());
        return true;
    }

    // .dq history [player]
    static bool HandleDQHistoryCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        std::string status = sDQMgr->GetStatusString(target);
        handler->SendSysMessage(status.c_str());
        return true;
    }

    // .dq history clear [player]
    static bool HandleDQHistoryClearCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->ClearHistory(target);
        handler->PSendSysMessage("DQ+ history cleared for {}.", target->GetName());
        return true;
    }

    // .dq episode list [zone_id]
    static bool HandleDQEpisodeListCommand(ChatHandler* handler, Optional<uint32> /*zoneId*/)
    {
        const auto& all = sInteractionLib->GetAll();
        uint32 count = 0;
        for (const auto& tmpl : all)
        {
            if (tmpl.tier != 2)
                continue;
            const char* mechName =
                (tmpl.mechanicType == DQ_MECHANIC_SUCCUBUS)     ? "succubus" :
                (tmpl.mechanicType == DQ_MECHANIC_HUNGRY_CHILD) ? "hungry_child" : "unknown";
            handler->PSendSysMessage("  id={} name='{}' mechanic={} level={}-{}",
                tmpl.id, tmpl.name, mechName, tmpl.levelMin, tmpl.levelMax);
            ++count;
        }
        if (count == 0)
            handler->SendSysMessage("No tier-2 (episode) templates loaded. Import interaction SQL and .dq reload.");
        else
            handler->PSendSysMessage("{} episode template(s) listed.", count);
        return true;
    }

    // .dq episode skip [player]
    static bool HandleDQEpisodeSkipCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->ForceAdvanceEpisode(target);
        handler->PSendSysMessage("DQ+ episode phase advanced for {}.", target->GetName());
        return true;
    }

    // .dq episode fail [player]
    static bool HandleDQEpisodeFailCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->OnInteractionFailed(target);
        handler->PSendSysMessage("DQ+ episode force-failed for {}.", target->GetName());
        return true;
    }

    // .dq episode spawn #id [player]
    static bool HandleDQEpisodeSpawnCommand(ChatHandler* handler,
        uint32 templateId, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        const InteractionTemplate* tmpl = sInteractionLib->GetById(templateId);
        if (!tmpl || tmpl->tier != 2)
        {
            handler->PSendSysMessage("Template {} is not a tier-2 episode.", templateId);
            return false;
        }

        sDQMgr->ForceTriggger(target, templateId);
        handler->PSendSysMessage("DQ+ episode {} spawned for {}.", templateId, target->GetName());
        return true;
    }

    // .dq debug on [player]
    static bool HandleDQDebugOnCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->SetDebugMode(target, true);
        handler->PSendSysMessage("DQ+ debug mode ON for {}.", target->GetName());
        return true;
    }

    // .dq debug off [player]
    static bool HandleDQDebugOffCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        sDQMgr->SetDebugMode(target, false);
        handler->PSendSysMessage("DQ+ debug mode OFF for {}.", target->GetName());
        return true;
    }

    // .dq debug gate [player]
    static bool HandleDQDebugGateCommand(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        Player* target = ResolveTarget(handler, targetArg);
        if (!target)
            return false;

        std::string gateStr = sDQMgr->GetGateString(target);
        handler->SendSysMessage(gateStr.c_str());
        return true;
    }

    // .dq catalogue zone #zone_id
    static bool HandleDQCatalogueZoneCommand(ChatHandler* handler, uint32 zoneId)
    {
        if (!sWorldCatalogue->IsLoaded())
        {
            handler->SendErrorMessage("World catalogue not loaded. Run .dq bootstrap first.");
            return false;
        }

        auto entries = sWorldCatalogue->GetZoneEntries(zoneId);
        handler->PSendSysMessage("Zone {}: {} catalogue entries", zoneId, entries.size());

        uint32 shown = 0;
        for (const auto* e : entries)
        {
            if (shown++ >= 10) break;
            handler->PSendSysMessage("  entry={} tags=[{}] threat={}",
                e->entry, e->tags, e->threatLevel);
        }
        if (entries.size() > 10)
            handler->PSendSysMessage("  ... and {} more.", entries.size() - 10);

        return true;
    }

    // .dq catalogue query tags[,tags] [zone_id]
    static bool HandleDQCatalogueQueryCommand(ChatHandler* handler,
        std::string tagsCsv, Optional<uint32> zoneId)
    {
        if (!sWorldCatalogue->IsLoaded())
        {
            handler->SendErrorMessage("World catalogue not loaded.");
            return false;
        }

        std::vector<std::string> required;
        std::stringstream ss(tagsCsv);
        std::string tok;
        while (std::getline(ss, tok, ','))
            required.push_back(tok);

        uint32 zone = zoneId.value_or(0);
        auto candidates = sWorldCatalogue->GetNPCCandidates(zone, required, {}, 3);

        handler->PSendSysMessage("Query tags=[{}] zone={}: {} results",
            tagsCsv, zone, candidates.size());

        uint32 shown = 0;
        for (uint32 entry : candidates)
        {
            if (shown++ >= 10) break;
            handler->PSendSysMessage("  entry={}", entry);
        }
        return true;
    }

    // .dq reload
    static bool HandleDQReloadCommand(ChatHandler* handler)
    {
        sDQMgr->ReloadTemplates();
        handler->SendSysMessage("DQ+ templates reloaded from DB.");
        return true;
    }

    // .dq bootstrap
    static bool HandleDQBootstrapCommand(ChatHandler* handler)
    {
        handler->SendSysMessage("DQ+ bootstrap: Running world catalogue scan...");
        WorldDatabase.Execute("CALL dq_bootstrap_catalogue()");
        WorldDatabase.Execute("CALL dq_bootstrap_tier1_quests()");
        handler->SendSysMessage("DQ+ bootstrap: Complete. Run .dq reload to load new data.");
        return true;
    }

private:
    static Player* ResolveTarget(ChatHandler* handler, Optional<PlayerIdentifier> targetArg)
    {
        if (targetArg)
        {
            if (Player* p = targetArg->GetConnectedPlayer())
                return p;
            handler->SendErrorMessage("Player not found or not online.");
            return nullptr;
        }

        Player* self = handler->GetPlayer();
        if (!self)
        {
            handler->SendErrorMessage("This command requires a target player (or be used in-game).");
            return nullptr;
        }
        return self;
    }
};

void AddSC_DQ_CommandScript()
{
    new DQ_CommandScript();
}
