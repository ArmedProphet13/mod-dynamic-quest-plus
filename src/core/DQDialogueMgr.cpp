/*
 * DynamicQuests+ Module
 * Phase 6: Dialogue System -- implementation
 */

#include "DQDialogueMgr.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "GossipDef.h"
#include "DQLog.h"
#include "MechanicUtils.h"
#include "Player.h"
#include "QueryResult.h"
#include "Random.h"
#include "ScriptedGossip.h"
#include "Timer.h"

DQDialogueMgr* DQDialogueMgr::instance()
{
    static DQDialogueMgr instance;
    return &instance;
}

// ---------------------------------------------------------------------------
// LoadFromDB
// ---------------------------------------------------------------------------

void DQDialogueMgr::LoadFromDB()
{
    _variants.clear();
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query(
        "SELECT emotion, context_tags, text_type, text "
        "FROM dq_text_variant_pool");

    if (!result)
    {
        DQ_LOG_WARN(DQ_LOG_CAT_STATE, nullptr, "DQDialogueMgr: dq_text_variant_pool is empty.");
        _loaded = true;
        return;
    }

    do
    {
        Field* f = result->Fetch();
        TextVariant tv;
        tv.emotion     = f[0].Get<std::string>();
        tv.contextTags = f[1].Get<std::string>();
        tv.textType    = f[2].Get<std::string>();
        tv.text        = f[3].Get<std::string>();
        _variants.push_back(std::move(tv));
    } while (result->NextRow());

    _loaded = true;
    DQ_LOG_INFO(DQ_LOG_CAT_STATE, nullptr, "DQDialogueMgr: Loaded {} text variants in {} ms.",
        _variants.size(), GetMSTimeDiffToNow(oldMSTime));
}

// ---------------------------------------------------------------------------
// GetVariantText
// ---------------------------------------------------------------------------

std::string DQDialogueMgr::GetVariantText(const std::string& emotion,
                                            const std::string& contextTags,
                                            const std::string& textType) const
{
    std::vector<const TextVariant*> matches;

    // First pass: require exact contextTags match (bonus specificity).
    if (!contextTags.empty())
    {
        for (const TextVariant& tv : _variants)
        {
            if (tv.emotion == emotion && tv.textType == textType && tv.contextTags == contextTags)
                matches.push_back(&tv);
        }
    }

    // Second pass: emotion + textType only (ignore contextTags).
    if (matches.empty())
    {
        for (const TextVariant& tv : _variants)
        {
            if (tv.emotion == emotion && tv.textType == textType)
                matches.push_back(&tv);
        }
    }

    if (matches.empty())
        return {};

    return matches[urand(0, static_cast<uint32>(matches.size()) - 1)]->text;
}

// ---------------------------------------------------------------------------
// BuildBeatMenu
// ---------------------------------------------------------------------------

void DQDialogueMgr::BuildBeatMenu(Player* player,
                                   const ArchetypeDef& def, const ArchetypeBeat& beat)
{
    DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "BuildBeatMenu: archetype={} beat={} mechanic={} passive={}",
        def.id, beat.beatNumber,
        static_cast<int>(beat.mechanic), static_cast<int>(beat.mechanicPassive));

    ClearGossipMenuFor(player);

    if (def.pattern == DQ_PATTERN_BRANCHING)
    {
        // Each beat after the current one is a branch; its textGreeting labels the button.
        uint8 choiceIdx = 0;
        for (const ArchetypeBeat& b : def.beats)
        {
            if (b.beatNumber <= beat.beatNumber || choiceIdx >= 4)
                continue;
            const char* label = b.textGreeting.empty() ? "..." : b.textGreeting.c_str();
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, DQ_COURIER_SENDER, choiceIdx);
            ++choiceIdx;
        }
    }
    else if (beat.mechanicPassive == 0)
    {
        // Active mechanics: one accept button whose text reflects the mechanic.
        const char* acceptText = nullptr;

        switch (beat.mechanic)
        {
            case DQ_BEAT_WITNESS:
                acceptText = "I see.";
                break;
            case DQ_BEAT_COURIER:
                if (beat.itemPrereqClass != 0)
                {
                    // Show accept only when the player actually carries the required item.
                    if (DQMechanicUtils::FindFirstItemByClass(player,
                            beat.itemPrereqClass, beat.itemPrereqSubclass))
                        acceptText = "Here, take this.";
                }
                else
                {
                    acceptText = "I'll see to it.";
                }
                break;
            case DQ_BEAT_KILL:
                acceptText = "I'll handle it.";
                break;
            case DQ_BEAT_GOTO:
                acceptText = "I'm on my way.";
                break;
            case DQ_BEAT_ACTIVATE:
                acceptText = "On it.";
                break;
            case DQ_BEAT_FIGHT:
                acceptText = "Let's do this.";
                break;
            default:
                break;
        }

        if (acceptText)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, acceptText, DQ_COURIER_SENDER, 0);
    }
    else
    {
        // Passive mechanic (e.g. CAST/heal): "Understood." transitions to ON_QUEST so
        // the passive hook can detect the cast.  Beat completes via DQ_PassiveHooks,
        // not via a second button click.
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Understood.", DQ_COURIER_SENDER, 0);
    }

    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Not right now.", DQ_COURIER_SENDER, DQ_GOSSIP_DECLINE);
}
