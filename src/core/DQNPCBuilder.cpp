/*
 * DynamicQuests+ Module
 * Phase 2: NPC Builder — implementation
 */

#include "DQNPCBuilder.h"
#include "WorldCatalogue.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Unit.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DQNPCBuilder::ParseAppearanceTags(const std::string& appearance,
                                        std::vector<std::string>& out)
{
    if (appearance.empty())
        return;

    size_t start = 0, end;
    while ((end = appearance.find(',', start)) != std::string::npos)
    {
        std::string tok = appearance.substr(start, end - start);
        while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
        while (!tok.empty() && tok.back()  == ' ') tok.pop_back();
        if (!tok.empty())
            out.push_back(std::move(tok));
        start = end + 1;
    }
    std::string tok = appearance.substr(start);
    while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
    while (!tok.empty() && tok.back()  == ' ') tok.pop_back();
    if (!tok.empty())
        out.push_back(std::move(tok));
}

// ---------------------------------------------------------------------------
// BuildProfile
// ---------------------------------------------------------------------------

DQNPCProfile DQNPCBuilder::BuildProfile(const ArchetypeDef& def,
                                         const ArchetypeBeat& beat,
                                         Player* player)
{
    DQNPCProfile profile;
    DQSpawnDesc& desc = profile.spawnDesc;

    // --- Entry + DisplayId -----------------------------------------------
    if (beat.displayId != 0)
    {
        // Explicit per-beat model override — use the social courier as the
        // base creature (for stats/AI) but swap its display to the given ID.
        desc.entry     = sWorldCatalogue->GetCourierEntryForTheme("social");
        desc.displayId = beat.displayId;
    }
    else
    {
        // Resolve entry and display from the archetype's appearance tags.
        std::vector<std::string> tags;
        ParseAppearanceTags(def.appearance, tags);

        const std::string& theme = tags.empty() ? "social" : tags[0];
        uint32 zoneId = (beat.zoneId != 0) ? beat.zoneId : def.zoneId;

        desc.entry     = sWorldCatalogue->GetCourierEntryForTheme(theme);
        desc.displayId = sWorldCatalogue->GetWorldCourierDisplayId(
            zoneId, tags, {}, 1 /*maxThreatLevel*/);
    }

    // --- Level scaling ---------------------------------------------------
    // DQSpawnSystem::ApplyScaling reads scaleLevel + levelOffset together.
    desc.scaleLevel  = true;
    desc.levelOffset = beat.npcLevelOffset;

    // --- Faction (start neutral; Fight Handler flips it when needed) ------
    desc.faction      = 0;     // 0 = keep DB default (courier entry is peaceful)
    desc.attackPlayer = false; // Fight Handler calls AI()->AttackStart

    // --- HP / damage (use DB stats; Fight Handler adjusts HP via SetHPPercent) --
    desc.hpRatio  = 0.0f;
    desc.dmgRatio = 0.0f;

    // --- Phase slots + animation spells ----------------------------------
    profile.concedePct = beat.fightThreshold;
    profile.entrySpell = beat.entrySpell;
    profile.exitSpell  = beat.exitSpell;
    profile.auraSpell  = beat.auraSpell;

    return profile;
}

// ---------------------------------------------------------------------------
// FlipToHostile
// ---------------------------------------------------------------------------

void DQNPCBuilder::FlipToHostile(Creature* npc, Player* player)
{
    if (!npc || !player)
        return;

    npc->SetFaction(FACTION_MONSTER);
    npc->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    npc->RemoveUnitFlag(UNIT_FLAG_PACIFIED);

    if (npc->AI())
        npc->AI()->AttackStart(player);
}

// ---------------------------------------------------------------------------
// FlipToFriendly
// ---------------------------------------------------------------------------

void DQNPCBuilder::FlipToFriendly(Creature* npc)
{
    if (!npc)
        return;

    npc->CombatStop(true);
    npc->SetFaction(FACTION_FRIENDLY);
    npc->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
}

// ---------------------------------------------------------------------------
// SetHPPercent
// ---------------------------------------------------------------------------

void DQNPCBuilder::SetHPPercent(Creature* npc, uint8 percent)
{
    if (!npc || percent == 0)
        return;

    uint32 newHp = static_cast<uint32>(npc->GetMaxHealth() * percent / 100.0f);
    if (newHp < 1)
        newHp = 1;
    if (newHp > npc->GetMaxHealth())
        newHp = npc->GetMaxHealth();

    npc->SetHealth(newHp);
}
