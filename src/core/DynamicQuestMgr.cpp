/*
 * DynamicQuests+ Module
 * Central Manager — Implementation
 */

#include "DynamicQuestMgr.h"
#include "DQSpawnSystem.h"
#include "AreaDefines.h"
#include "WorldCatalogue.h"
#include "InteractionTemplateLibrary.h"
#include "NPCMatchingEngine.h"
#include "MechanicSuccubus.h"
#include "MechanicHungryChild.h"
#include "MechanicArchetype.h"
#include "ArchetypeMgr.h"
#include "DQEmotionEngine.h"
#include "Chat.h"
#include "Config.h"
#include "Field.h"
#include "QueryResult.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "WorldSession.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"
#include <cmath>
#include <random>
#include <sstream>
#include <fmt/format.h>

// Returns the faction alignment of a capital city by zone/area ID.
// Uses AreaDefines.h constants — no magic numbers.
static DQZoneFaction GetZoneFaction(uint32 zoneId, uint32 areaId)
{
    auto classify = [](uint32 id) -> DQZoneFaction {
        switch (id)
        {
            case AREA_STORMWIND_CITY:
            case AREA_IRONFORGE:
            case AREA_DARNASSUS:
            case AREA_THE_EXODAR:
                return DQ_FACTION_ALLIANCE;
            case AREA_UNDERCITY:
            case AREA_ORGRIMMAR:
            case AREA_THUNDER_BLUFF:
            case AREA_SILVERMOON_CITY:
                return DQ_FACTION_HORDE;
            case AREA_DALARAN:
            case AREA_SHATTRATH_CITY:
                return DQ_FACTION_NEUTRAL;
            default:
                return DQ_FACTION_ANY;
        }
    };
    DQZoneFaction zf = classify(zoneId);
    if (zf != DQ_FACTION_ANY) return zf;
    return classify(areaId); // fallback: check sub-area (Silvermoon, Exodar sub-areas)
}

// Splits a comma-separated tag expression (e.g. "humanoid,male,peasant") into a vector.
static std::vector<std::string> SplitTags(const std::string& tagExpr)
{
    std::vector<std::string> out;
    if (tagExpr.empty())
        return out;
    std::istringstream ss(tagExpr);
    std::string tok;
    while (std::getline(ss, tok, ','))
    {
        size_t l = tok.find_first_not_of(' ');
        size_t r = tok.find_last_not_of(' ');
        if (l != std::string::npos)
            out.push_back(tok.substr(l, r - l + 1));
    }
    return out;
}

static std::string FormatDuration(uint32 ms)
{
    uint32 secs = ms / 1000;
    uint32 mins = secs / 60;
    secs %= 60;
    if (mins > 0)
        return fmt::format("{}m {}s", mins, secs);
    return fmt::format("{}s", secs);
}

DynamicQuestMgr* DynamicQuestMgr::instance()
{
    static DynamicQuestMgr instance;
    return &instance;
}

void DynamicQuestMgr::Initialize()
{
    LoadConfig();
    sDQEmotions->Initialize();
    sDQContext->Initialize();
    sWorldCatalogue->LoadFromDB();
    sInteractionLib->LoadFromDB();
    sArchetypeMgr->LoadFromDB();
    RegisterMechanics();

    LOG_INFO("module.dynamicquests",
        "DynamicQuests+: Initialized. Templates={} CatalogueEntries={} Archetypes={} Mechanics={}",
        sInteractionLib->GetCount(), sWorldCatalogue->GetEntryCount(),
        sArchetypeMgr->GetCount(), _mechanics.size());
}

void DynamicQuestMgr::RegisterMechanics()
{
    _mechanics[DQ_MECHANIC_SUCCUBUS]     = std::make_unique<MechanicSuccubus>();
    _mechanics[DQ_MECHANIC_HUNGRY_CHILD] = std::make_unique<MechanicHungryChild>();
    _mechanics[DQ_MECHANIC_ARCHETYPE]    = std::make_unique<MechanicArchetype>();
}

void DynamicQuestMgr::LoadConfig()
{
    cfg_enabled          = sConfigMgr->GetOption<bool>  ("DynamicQuests.Enable", true);
    cfg_tier1MinMs       = sConfigMgr->GetOption<uint32>("DynamicQuests.Tier1MinTimerSeconds", 1800) * 1000;
    cfg_tier1MaxMs       = sConfigMgr->GetOption<uint32>("DynamicQuests.Tier1MaxTimerSeconds", 2700) * 1000;
    cfg_tier2MinMs       = sConfigMgr->GetOption<uint32>("DynamicQuests.Tier2MinTimerSeconds", 5400) * 1000;
    cfg_tier2MaxMs       = sConfigMgr->GetOption<uint32>("DynamicQuests.Tier2MaxTimerSeconds", 9000) * 1000;
    cfg_activityWindowMs = sConfigMgr->GetOption<uint32>("DynamicQuests.ActivityWindowSeconds", 30) * 1000;
    cfg_historySize      = sConfigMgr->GetOption<uint32>("DynamicQuests.HistorySize", 10);
    cfg_tier2Chance      = sConfigMgr->GetOption<uint32>("DynamicQuests.Tier2Chance", 20);
    cfg_courierSpeedBonus = sConfigMgr->GetOption<float>("DynamicQuests.CourierSpeedBonus", 1.25f);
    cfg_courierTimeoutMs = sConfigMgr->GetOption<uint32>("DynamicQuests.CourierTimeoutSeconds", 45) * 1000;
    cfg_verbose          = sConfigMgr->GetOption<bool>  ("DynamicQuests.VerboseLogging", false);
}

void DynamicQuestMgr::ReloadTemplates()
{
    sWorldCatalogue->LoadFromDB();
    sInteractionLib->LoadFromDB();
    LOG_INFO("module.dynamicquests", "DynamicQuests+: Templates reloaded. Templates={} CatalogueEntries={}",
        sInteractionLib->GetCount(), sWorldCatalogue->GetEntryCount());
}

// ---------------------------------------------------------------------------
// Per-player lifecycle
// ---------------------------------------------------------------------------

void DynamicQuestMgr::OnPlayerUpdate(Player* player, uint32 diff)
{
    if (!cfg_enabled)
        return;

    PlayerDQState& ps = GetOrCreate(player->GetGUID());

    // Update PlayerContext first
    PlayerContextObserver::Update(player, ps.ctx, diff);

    switch (ps.state)
    {
        case DQ_STATE_IDLE:
            TryTrigger(player, ps);
            break;

        case DQ_STATE_COOLDOWN:
            TickCooldown(player, ps, diff);
            break;

        case DQ_STATE_INBOUND:
            TickInbound(player, ps, diff);
            break;

        case DQ_STATE_ON_QUEST:
            TickOnQuest(player, ps, diff);
            break;

        default:
            break;
    }
}

void DynamicQuestMgr::OnPlayerZoneChange(Player* player, uint32 newZone, uint32 newArea)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    PlayerContextObserver::OnZoneChanged(player, ps.ctx, newZone, newArea);
}

void DynamicQuestMgr::OnPlayerKill(Player* player, Creature* victim)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    PlayerContextObserver::OnKill(ps.ctx);

    if (ps.state == DQ_STATE_ON_QUEST)
        if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
            m->OnKill(player, victim, ps.activeInst);
}

void DynamicQuestMgr::OnPlayerLoot(Player* player, uint32 itemId)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    PlayerContextObserver::OnLoot(player, ps.ctx, itemId);

    if (ps.state == DQ_STATE_ON_QUEST)
        if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
            m->OnLoot(player, itemId, ps.activeInst);
}

void DynamicQuestMgr::OnPlayerLogout(Player* player)
{
    AbortInteraction(player);

    const PlayerDQState& ps = GetOrCreate(player->GetGUID());

    // Persist history to character_dq_history
    uint32 now     = static_cast<uint32>(GameTime::GetGameTime().count());
    uint32 nextTrigger = now + (ps.cooldownRemainMs / 1000);

    uint32 h[10] = {};
    size_t slot = 0;
    for (uint32 id : ps.history)
    {
        if (slot >= 10) break;
        h[slot++] = id;
    }

    CharacterDatabase.Execute(
        "INSERT INTO character_dq_history "
        "(guid,last_template_1,last_template_2,last_template_3,last_template_4,"
        "last_template_5,last_template_6,last_template_7,last_template_8,"
        "last_template_9,last_template_10,"
        "last_courier_theme,last_episode_id,next_trigger_time,consecutive_tier1,"
        "ileana_episode,soul_debt_until) "
        "VALUES ({},{},{},{},{},{},{},{},{},{},{},'{}',{},{},{},{},{}) "
        "ON DUPLICATE KEY UPDATE "
        "last_template_1={},last_template_2={},last_template_3={},last_template_4={},"
        "last_template_5={},last_template_6={},last_template_7={},last_template_8={},"
        "last_template_9={},last_template_10={},"
        "last_courier_theme='{}',last_episode_id={},next_trigger_time={},consecutive_tier1={},"
        "ileana_episode={},soul_debt_until={}",
        player->GetGUID().GetCounter(),
        h[0],h[1],h[2],h[3],h[4],h[5],h[6],h[7],h[8],h[9],
        ps.lastCourierTheme, ps.lastEpisodeId, nextTrigger, ps.consecutiveTier1,
        ps.ileanaEpisode, ps.soulDebtUntil,
        h[0],h[1],h[2],h[3],h[4],h[5],h[6],h[7],h[8],h[9],
        ps.lastCourierTheme, ps.lastEpisodeId, nextTrigger, ps.consecutiveTier1,
        ps.ileanaEpisode, ps.soulDebtUntil);

    _states.erase(player->GetGUID().GetRawValue());

    LOG_DEBUG("module.dynamicquests", "Player {} logged out. DQ state saved.", player->GetName());
}

void DynamicQuestMgr::OnPlayerLogin(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ps.ctx.zoneId = player->GetZoneId();
    ps.ctx.areaId = player->GetAreaId();
    ps.ctx.mapId  = player->GetMapId();
    ps.ctx.level  = static_cast<uint8>(player->GetLevel());

    // Safe default — overwritten by the async callback below once the DB responds.
    ps.cooldownRemainMs = RollCooldown(1);
    TransitionState(player, ps, DQ_STATE_COOLDOWN);

    // Fire the load as an async query so we never block the map thread or
    // collide with another player's login on the single synchronous connection.
    // The callback is dispatched back to the world thread via the session
    // QueryProcessor, so accessing Player* and game state inside it is safe.
    ObjectGuid playerGuid = player->GetGUID();
    std::string loginSql = fmt::format(
        "SELECT last_template_1,last_template_2,last_template_3,last_template_4,"
        "last_template_5,last_template_6,last_template_7,last_template_8,"
        "last_template_9,last_template_10,"
        "last_courier_theme,last_episode_id,next_trigger_time,consecutive_tier1,"
        "ileana_episode,soul_debt_until "
        "FROM character_dq_history WHERE guid={}",
        player->GetGUID().GetCounter());

    player->GetSession()->GetQueryProcessor().AddCallback(
        CharacterDatabase.AsyncQuery(loginSql).WithCallback(
            [playerGuid](QueryResult result)
            {
                sDQMgr->ApplyLoginData(playerGuid, std::move(result));
            })
    );

    // Load archetype beat progress (separate table, separate async query)
    std::string archSql = fmt::format(
        "SELECT archetype_id, current_beat, completed "
        "FROM character_dq_sequences WHERE guid={}",
        player->GetGUID().GetCounter());

    player->GetSession()->GetQueryProcessor().AddCallback(
        CharacterDatabase.AsyncQuery(archSql).WithCallback(
            [playerGuid](QueryResult result)
            {
                sDQMgr->ApplyArchetypeData(playerGuid, std::move(result));
            })
    );

    LOG_DEBUG("module.dynamicquests", "Player {} logged in. DQ data loading async.",
        player->GetName());
}

// ---------------------------------------------------------------------------
// Courier events
// ---------------------------------------------------------------------------

void DynamicQuestMgr::OnCourierArrived(Player* player, Creature* courier)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    if (ps.state != DQ_STATE_INBOUND)
        return;

    // Set up the interaction instance for this delivery
    ps.activeInst = {};
    ps.activeInst.templateId  = ps.activeTemplateId;
    ps.activeInst.courierGuid = courier ? courier->GetGUID() : ps.courierGuid;
    ps.activeInst.phaseBit    = ps.activePhaseBit; // propagate to mechanics for private spawns

    TransitionState(player, ps, DQ_STATE_DELIVERING);

    if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
        m->OnStart(player, courier, ps.activeInst);

    LOG_INFO("module.dynamicquests", "Courier arrived for player {}. Template={}",
        player->GetName(), ps.activeTemplateId);
}

void DynamicQuestMgr::OnInteractionAccepted(Player* player, uint32 templateId, uint32 choiceIdx)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ps.activeTemplateId         = templateId;
    ps.activeInst.templateId    = templateId;
    ps.activeInst.choiceIndex   = choiceIdx;
    ps.activeInst.courierGuid   = ps.courierGuid;

    TransitionState(player, ps, DQ_STATE_ON_QUEST);

    if (IMechanicModule* m = GetMechanic(templateId))
        m->OnChoice(player, choiceIdx, ps.activeInst);

    LOG_INFO("module.dynamicquests", "Player {} accepted interaction template={} choice={}",
        player->GetName(), templateId, choiceIdx);
}

void DynamicQuestMgr::OnInteractionDeclined(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ReleasePhase(player, ps);
    TransitionState(player, ps, DQ_STATE_COOLDOWN);
    ps.cooldownRemainMs = RollCooldown(1);
    LOG_INFO("module.dynamicquests", "Player {} declined interaction. Cooldown={}s",
        player->GetName(), ps.cooldownRemainMs / 1000);
}

void DynamicQuestMgr::OnInteractionComplete(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ReleasePhase(player, ps);

    // Archetypes use character_dq_sequences for progression — only add to template
    // history when the full arc is done so the archetype can re-trigger for later beats.
    if (IsArchetypeId(ps.activeTemplateId))
    {
        if (ps.activeInst.completed)
            AddToHistory(ps, ps.activeTemplateId);
    }
    else
    {
        AddToHistory(ps, ps.activeTemplateId);
    }

    const InteractionTemplate* tmpl = sInteractionLib->GetById(ps.activeTemplateId);
    uint8 tier = tmpl ? tmpl->tier : 1;

    // Advance the Ileana encounter counter when an episode completes.
    if (ps.activeTemplateId >= 10 && ps.activeTemplateId <= 12)
        ps.ileanaEpisode = std::min<uint8>(ps.ileanaEpisode + 1, 3);

    if (tier == 1)
        ps.consecutiveTier1++;
    else
    {
        ps.consecutiveTier1 = 0;
        ps.lastEpisodeId = ps.activeTemplateId;
    }

    ps.activeTemplateId = 0;
    ps.activeQuestId    = 0;

    TransitionState(player, ps, DQ_STATE_COOLDOWN);
    ps.cooldownRemainMs = RollCooldown(tier);

    LOG_INFO("module.dynamicquests", "Player {} completed interaction. tier={} cooldown={}s",
        player->GetName(), tier, ps.cooldownRemainMs / 1000);
}

void DynamicQuestMgr::OnInteractionFailed(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ReleasePhase(player, ps);
    ps.activeTemplateId = 0;
    ps.activeQuestId    = 0;
    TransitionState(player, ps, DQ_STATE_COOLDOWN);
    ps.cooldownRemainMs = RollCooldown(1);
}

// ---------------------------------------------------------------------------
// GM Command API
// ---------------------------------------------------------------------------

void DynamicQuestMgr::ForceTriggger(Player* player, uint32 templateId)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());

    // Bypass all gate checks; reset any active state
    if (ps.state == DQ_STATE_INBOUND || ps.state == DQ_STATE_DELIVERING)
        AbortInteraction(player);

    ps.cooldownRemainMs = 0;

    uint32 selectedTemplate = templateId;

    if (selectedTemplate == 0)
    {
        uint8  playerLevel = static_cast<uint8>(player->GetLevel());
        uint32 playerZone  = player->GetZoneId();

        // Regular templates — pick first level-matching one as fallback
        for (const auto& tmpl : sInteractionLib->GetAll())
        {
            if (tmpl.levelMin <= playerLevel && tmpl.levelMax >= playerLevel)
            {
                selectedTemplate = tmpl.id;
                break;
            }
        }

        // Archetypes — prefer over regular templates when eligible in current zone
        PlayerContext fctx;
        fctx.zoneId     = playerZone;
        fctx.areaId     = player->GetAreaId();
        fctx.level      = playerLevel;
        fctx.race       = player->getRace();
        fctx.isOutdoors = player->IsOutdoors();
        fctx.isInCombat = player->IsInCombat();
        fctx.isSwimming = player->IsInWater();
        DQTagSet forceTags = sDQContext->Resolve(player, fctx);
        std::vector<uint32> archetypeIds = sArchetypeMgr->GetEligible(playerZone, playerLevel, forceTags);
        for (uint32 arcId : archetypeIds)
        {
            auto it = ps.archetypeProgress.find(arcId);
            if (it != ps.archetypeProgress.end() && it->second == 0xFF)
                continue; // already completed
            selectedTemplate = EncodeArchetypeId(arcId);
            break;
        }

        if (selectedTemplate == 0)
        {
            LOG_ERROR("module.dynamicquests", "ForceTriggger: No template found for player {} (level={}, zone={}).",
                player->GetName(), playerLevel, playerZone);
            return;
        }
    }

    // Archetype spawn path
    if (IsArchetypeId(selectedTemplate))
    {
        uint32 archetypeId = DecodeArchetypeId(selectedTemplate);
        CourierSelection sel;
        sel.entry = sWorldCatalogue->GetCourierEntryForTheme("social");
        if (sel.entry == 0)
        {
            LOG_WARN("module.dynamicquests", "ForceTriggger: No social courier entry for archetype {}.",
                archetypeId);
            return;
        }
        if (const ArchetypeDef* def = sArchetypeMgr->Get(archetypeId))
            if (!def->appearance.empty())
                sel.displayId = sWorldCatalogue->GetWorldCourierDisplayId(
                    ps.ctx.zoneId, SplitTags(def->appearance), {}, 1);
        SpawnCourier(player, ps, selectedTemplate, sel);
        return;
    }

    const InteractionTemplate* tmpl = sInteractionLib->GetById(selectedTemplate);
    if (!tmpl)
        return;

    CourierSelection sel = NPCMatchingEngine::FindBestCourier(*tmpl, ps.ctx);

    if (sel.entry == 0)
    {
        LOG_WARN("module.dynamicquests", "ForceTriggger: No courier entry found for template {}.", selectedTemplate);
        return;
    }

    SpawnCourier(player, ps, selectedTemplate, sel);
}

void DynamicQuestMgr::AbortInteraction(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ReleasePhase(player, ps);

    // Notify mechanic to clean up any spawns
    if (ps.activeTemplateId != 0)
        if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
            m->OnCleanup(player, ps.activeInst);

    if (!ps.courierGuid.IsEmpty())
    {
        if (Creature* courier = player->GetMap()->GetCreature(ps.courierGuid))
            courier->DespawnOrUnsummon(Milliseconds(0));
        ps.courierGuid = ObjectGuid::Empty;
    }

    ps.activeTemplateId = 0;
    ps.activeQuestId    = 0;
    ps.courierTimeoutMs = 0;
    ps.activeInst       = {};
    TransitionState(player, ps, DQ_STATE_IDLE);
}

std::string DynamicQuestMgr::GetStatusString(Player* player) const
{
    const PlayerDQState* ps = Get(player->GetGUID());
    if (!ps)
        return "No DQ state for this player.";

    std::ostringstream ss;
    ss << "=== DynamicQuests+ | Player: " << player->GetName() << " ===\n";
    ss << "State      : " << DQPlayerStateStr(ps->state);
    if (ps->state == DQ_STATE_COOLDOWN)
        ss << " (cooldown: " << FormatDuration(ps->cooldownRemainMs) << ")";
    ss << "\n";

    const char* ctxStr[] = { "IDLE", "TRAVELING", "GRINDING", "EXPLORING" };
    ss << "Context    : " << ctxStr[ps->ctx.activityContext]
       << " (" << static_cast<int>(ps->ctx.distanceTraveled5min) << "y/5min"
       << ", " << ps->ctx.recentKills5min << " kills)\n";

    ss << "Zone/Area  : " << ps->ctx.zoneId << "/" << ps->ctx.areaId
       << ", " << (ps->ctx.isOutdoors ? "OUTDOOR" : "INDOORS") << "\n";
    ss << "Level/Gold : " << static_cast<int>(ps->ctx.level)
       << " / " << (ps->ctx.goldCopper / 10000) << "g "
       << ((ps->ctx.goldCopper % 10000) / 100) << "s\n\n";

    ss << "Gate checks:\n";
    ss << "  [" << (ps->lastGateResult.passNotAFK        ? "PASS" : "FAIL") << "] Not AFK\n";
    ss << "  [" << (ps->lastGateResult.passNotCombat     ? "PASS" : "FAIL") << "] Not in combat\n";
    ss << "  [" << (ps->lastGateResult.passOutdoors      ? "PASS" : "FAIL") << "] Outdoors (VMAP)\n";
    ss << "  [" << (ps->lastGateResult.passNotDungeon    ? "PASS" : "FAIL") << "] Not dungeon/raid/bg\n";
    ss << "  [" << (ps->lastGateResult.passNotFlying   ? "PASS" : "FAIL") << "] Not flying\n";
    ss << "  [" << (ps->lastGateResult.passNotSwimming ? "PASS" : "FAIL") << "] Not swimming\n";
    ss << "  [" << (ps->lastGateResult.passActive      ? "PASS" : "FAIL") << "] Active (moved recently)\n";
    ss << "  [" << (ps->lastGateResult.passCooldown    ? "PASS" : "FAIL") << "] Cooldown elapsed\n\n";

    ss << "History (last " << ps->history.size() << "): ";
    for (uint32 id : ps->history)
        ss << id << " ";
    ss << "\n";
    ss << "Last theme : " << ps->lastCourierTheme << "\n";
    ss << "Last episode: " << ps->lastEpisodeId << "\n";
    ss << "Tier2 escalation: " << static_cast<int>(ps->consecutiveTier1)
       << "/3 (boost at 3 consecutive tier 1)\n";

    return ss.str();
}

std::string DynamicQuestMgr::GetGateString(Player* player) const
{
    const PlayerDQState* ps = Get(player->GetGUID());
    if (!ps)
        return "No DQ state.";

    EligibilityResult r = EligibilityEngine::Check(ps->ctx, ps->cooldownRemainMs);

    std::ostringstream ss;
    ss << "Gate check for " << player->GetName() << ":\n";
    ss << "  [" << (r.passNotAFK        ? "PASS" : "FAIL") << "] !isAFK\n";
    ss << "  [" << (r.passNotCombat     ? "PASS" : "FAIL") << "] !IsInCombat\n";
    ss << "  [" << (r.passOutdoors      ? "PASS" : "FAIL") << "] isOutdoors\n";
    ss << "  [" << (r.passNotDungeon    ? "PASS" : "FAIL") << "] !isInDungeon\n";
    ss << "  [" << (r.passNotFlying   ? "PASS" : "FAIL") << "] !isFlying\n";
    ss << "  [" << (r.passNotSwimming ? "PASS" : "FAIL") << "] !isSwimming\n";
    ss << "  [" << (r.passActive      ? "PASS" : "FAIL") << "] hasMovedRecently\n";
    ss << "  [" << (r.passCooldown    ? "PASS" : "FAIL") << "] cooldown=0 (" << FormatDuration(ps->cooldownRemainMs) << " remain)\n";
    ss << "  => " << (r.eligible ? "ELIGIBLE" : "NOT ELIGIBLE") << "\n";
    return ss.str();
}

void DynamicQuestMgr::SetDebugMode(Player* player, bool enable)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ps.debugMode = enable;
}

void DynamicQuestMgr::ClearHistory(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ps.history.clear();
    ps.lastCourierTheme.clear();
    ps.lastEpisodeId    = 0;
    ps.consecutiveTier1 = 0;
    CharacterDatabase.Execute("DELETE FROM character_dq_history WHERE guid = {}",
        player->GetGUID().GetCounter());
    LOG_DEBUG("module.dynamicquests", "History cleared for player {}.", player->GetName());
}

void DynamicQuestMgr::ForceAdvanceEpisode(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    if (ps.state != DQ_STATE_ON_QUEST || ps.activeTemplateId == 0)
        return;
    if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
        m->ForceAdvance(player, ps.activeInst);
}

uint32 DynamicQuestMgr::GetActiveTemplateId(Player* player) const
{
    const PlayerDQState* ps = Get(player->GetGUID());
    return ps ? ps->activeTemplateId : 0;
}

const InteractionInstance* DynamicQuestMgr::GetActiveInst(Player* player) const
{
    const PlayerDQState* ps = Get(player->GetGUID());
    return ps ? &ps->activeInst : nullptr;
}

DQPlayerState DynamicQuestMgr::GetPlayerState(Player* player) const
{
    const PlayerDQState* ps = Get(player->GetGUID());
    return ps ? ps->state : DQ_STATE_IDLE;
}

void DynamicQuestMgr::OnCourierKilled(Player* player, Creature* courier)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    if (ps.state != DQ_STATE_ON_QUEST)
        return;

    if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
        m->OnKill(player, courier, ps.activeInst);
}

void DynamicQuestMgr::OnDestinationInteracted(Player* player, Creature* destNpc)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    if (ps.state != DQ_STATE_ON_QUEST)
        return;

    if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
        m->OnDelivery(player, destNpc, ps.activeInst);
}

void DynamicQuestMgr::OnPropActivated(Player* player, GameObject* go)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    if (ps.state != DQ_STATE_ON_QUEST)
        return;

    if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
        m->OnActivate(player, go, ps.activeInst);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

PlayerDQState& DynamicQuestMgr::GetOrCreate(ObjectGuid guid)
{
    return _states[guid.GetRawValue()];
}

const PlayerDQState* DynamicQuestMgr::Get(ObjectGuid guid) const
{
    auto it = _states.find(guid.GetRawValue());
    if (it == _states.end())
        return nullptr;
    return &it->second;
}

IMechanicModule* DynamicQuestMgr::GetMechanic(uint32 templateId) const
{
    if (IsArchetypeId(templateId))
    {
        auto it = _mechanics.find(static_cast<uint8>(DQ_MECHANIC_ARCHETYPE));
        return it != _mechanics.end() ? it->second.get() : nullptr;
    }
    const InteractionTemplate* tmpl = sInteractionLib->GetById(templateId);
    if (!tmpl)
        return nullptr;
    auto it = _mechanics.find(static_cast<uint8>(tmpl->mechanicType));
    if (it == _mechanics.end())
        return nullptr;
    return it->second.get();
}

void DynamicQuestMgr::TickOnQuest(Player* player, PlayerDQState& ps, uint32 diff)
{
    if (ps.activeTemplateId == 0)
        return;
    if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
        m->OnUpdate(player, diff, ps.activeInst);
}

void DynamicQuestMgr::TickCooldown(Player* /*player*/, PlayerDQState& ps, uint32 diff)
{
    if (ps.cooldownRemainMs <= diff)
    {
        ps.cooldownRemainMs = 0;
        ps.state = DQ_STATE_IDLE;
    }
    else
    {
        ps.cooldownRemainMs -= diff;
    }
}

void DynamicQuestMgr::TickInbound(Player* player, PlayerDQState& ps, uint32 diff)
{
    if (ps.courierTimeoutMs <= diff)
    {
        // Courier timed out — player moved away, zoned, went indoors
        LOG_DEBUG("module.dynamicquests.courier", "Courier timeout for player {}. Aborting.",
            player->GetName());
        AbortInteraction(player);
        ps.cooldownRemainMs = RollCooldown(1);
        ps.state = DQ_STATE_COOLDOWN;
        return;
    }
    ps.courierTimeoutMs -= diff;

    // Check if courier is still alive and in range (handled by DQ_CourierAI)
    if (!ps.courierGuid.IsEmpty())
    {
        Creature* courier = player->GetMap()->GetCreature(ps.courierGuid);
        if (!courier || !courier->IsAlive())
        {
            // Courier died or was removed by something else
            ps.courierGuid = ObjectGuid::Empty;
            AbortInteraction(player);
            ps.cooldownRemainMs = RollCooldown(1);
            ps.state = DQ_STATE_COOLDOWN;
        }
    }
}

void DynamicQuestMgr::TryTrigger(Player* player, PlayerDQState& ps)
{
    if (ps.cooldownRemainMs > 0)
        return;

    EligibilityResult gate = EligibilityEngine::Check(ps.ctx, 0);
    ps.lastGateResult = gate;

    if (!gate.eligible)
        return;

    DQTagSet tags = sDQContext->Resolve(player, ps.ctx);
    ps.lastResolvedTags = tags;

    const auto& all = sInteractionLib->GetAll();
    if (all.empty())
        return;

    uint8 contextScore = EligibilityEngine::GetContextScore(ps.ctx);

    uint32 tier2Pct = cfg_tier2Chance;
    if (ps.consecutiveTier1 >= 3)
        tier2Pct = std::min(50u, tier2Pct + 30);

    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<uint32> tierRoll(0, 99);
    uint8 targetTier = (tierRoll(rng) < tier2Pct) ? 2 : 1;

    // Filter candidates
    std::vector<std::pair<uint32, int32>> pool; // (templateId, weight)
    for (const auto& tmpl : all)
    {
        if (tmpl.tier != targetTier)
            continue;
        if (tmpl.levelMin > ps.ctx.level || tmpl.levelMax < ps.ctx.level)
            continue;
        if (tmpl.prereqGold > ps.ctx.goldCopper)
            continue;
        // Zone type check: capital templates only fire in capital cities;
        // outdoor templates are blocked inside capitals (the eligibility passOutdoors
        // gate already blocks buildings, so this handles zone category).
        {
            bool inCapital = (GetZoneFaction(ps.ctx.zoneId, ps.ctx.areaId) != DQ_FACTION_ANY);
            if (tmpl.zoneType == DQ_ZONE_CAPITAL && !inCapital)
                continue;
            if (tmpl.zoneType == DQ_ZONE_OUTDOOR && inCapital)
                continue;
        }
        // Zone faction check — prevents e.g. hungry_child (alliance) firing in Undercity
        if (tmpl.zoneFaction != DQ_FACTION_ANY)
        {
            DQZoneFaction zf = GetZoneFaction(ps.ctx.zoneId, ps.ctx.areaId);
            if (zf != DQ_FACTION_ANY && zf != tmpl.zoneFaction)
                continue;
        }

        // History check
        bool inHistory = false;
        for (uint32 hid : ps.history)
            if (hid == tmpl.id) { inHistory = true; break; }
        if (inHistory)
            continue;

        // Ileana episode gating: templates 10/11/12 require exact episode progression.
        // Template 10 = ep1 (ileanaEpisode must be 0), 11 = ep2 (must be 1), 12 = ep3 (must be 2).
        // Once all 3 are done (ileanaEpisode == 3) none of them may fire again.
        if (tmpl.id >= 10 && tmpl.id <= 12)
        {
            uint8 req = static_cast<uint8>(tmpl.id - 10); // 10→0, 11→1, 12→2
            if (ps.ileanaEpisode != req)
                continue;
        }

        // Context weight boost
        int32 weight = tmpl.rarityWeight;
        if (tmpl.contextMask & contextScore)
            weight = static_cast<int32>(weight * 1.5f);

        pool.push_back({ tmpl.id, weight });
    }

    // Fallback: if no tier2 found, try tier1
    if (pool.empty() && targetTier == 2)
    {
        targetTier = 1;
        for (const auto& tmpl : all)
        {
            if (tmpl.tier != 1) continue;
            if (tmpl.levelMin > ps.ctx.level || tmpl.levelMax < ps.ctx.level) continue;
            bool inHistory = false;
            for (uint32 hid : ps.history)
                if (hid == tmpl.id) { inHistory = true; break; }
            if (inHistory) continue;
            pool.push_back({ tmpl.id, tmpl.rarityWeight });
        }
    }

    // Add eligible archetypes to the pool.
    // Completed archetypes are excluded via the cached archetypeProgress map.
    // Archetypes bypass the history check — progression is managed by character_dq_sequences.
    {
        std::vector<uint32> archetypeIds = sArchetypeMgr->GetEligible(ps.ctx.zoneId, ps.ctx.level, tags);
        for (uint32 arcId : archetypeIds)
        {
            auto it = ps.archetypeProgress.find(arcId);
            if (it != ps.archetypeProgress.end() && it->second == 0xFF)
                continue; // completed
            pool.push_back({ EncodeArchetypeId(arcId), 10 });
        }
    }

    if (pool.empty())
        return;

    // Weighted random selection
    int32 totalWeight = 0;
    for (auto& p : pool)
        totalWeight += p.second;

    std::uniform_int_distribution<int32> weightRoll(0, totalWeight - 1);
    int32 roll = weightRoll(rng);
    uint32 selectedId = pool.back().first;
    for (auto& p : pool)
    {
        roll -= p.second;
        if (roll < 0) { selectedId = p.first; break; }
    }

    // Archetype: synthesise CourierSelection — appearance tags drive display ID selection.
    if (IsArchetypeId(selectedId))
    {
        uint32 archetypeId = DecodeArchetypeId(selectedId);
        CourierSelection sel;
        sel.entry = sWorldCatalogue->GetCourierEntryForTheme("social");
        if (sel.entry == 0)
        {
            LOG_WARN("module.dynamicquests",
                "TryTrigger: No social courier entry for archetype {}. Skipping.",
                archetypeId);
            return;
        }
        if (const ArchetypeDef* def = sArchetypeMgr->Get(archetypeId))
            if (!def->appearance.empty())
                sel.displayId = sWorldCatalogue->GetWorldCourierDisplayId(
                    ps.ctx.zoneId, SplitTags(def->appearance), {}, 1);
        SpawnCourier(player, ps, selectedId, sel);
        return;
    }

    const InteractionTemplate* tmpl = sInteractionLib->GetById(selectedId);
    if (!tmpl)
        return;

    // Find courier NPC
    CourierSelection sel = NPCMatchingEngine::FindBestCourier(*tmpl, ps.ctx);
    if (sel.entry == 0)
    {
        LOG_DEBUG("module.dynamicquests.nme", "TryTrigger: No courier found for template {}. Skipping.",
            selectedId);
        return;
    }

    SpawnCourier(player, ps, selectedId, sel);
}

bool DynamicQuestMgr::SpawnCourier(Player* player, PlayerDQState& ps,
    uint32 templateId, const CourierSelection& sel)
{
    // Acquire private phase before spawning so the creature is invisible to
    // bystanders from the moment it enters the world (no brief world-visible flash).
    // Must be released explicitly if spawning fails.
    uint32 phaseBit = _phasePool.Acquire();
    if (phaseBit == 0)
        LOG_WARN("module.dynamicquests",
            "Phase pool exhausted — courier for player {} is world-visible.", player->GetName());

    const InteractionTemplate* tmplPtr = sInteractionLib->GetById(templateId);
    bool isSuccubus     = (tmplPtr && tmplPtr->mechanicType == DQ_MECHANIC_SUCCUBUS);
    bool isHungryChild  = (tmplPtr && tmplPtr->mechanicType == DQ_MECHANIC_HUNGRY_CHILD);

    DQSpawnDesc desc;
    desc.entry      = sel.entry;
    desc.phaseBit   = phaseBit;
    desc.displayId  = sel.displayId;
    desc.scaleLevel = true;

    // HungryChild: override with a faction/race-appropriate humanoid child model.
    // Never cross-faction, never beast-looking. Returns 0 → skip quest.
    if (isHungryChild)
    {
        uint32 childDisplayId = MechanicHungryChild::GetChildDisplayId(player);
        if (childDisplayId == 0)
        {
            _phasePool.Release(phaseBit);
            LOG_DEBUG("module.dynamicquests.hungry_child",
                "HungryChild: no child model for race={}, skipping for player {}.",
                static_cast<uint32>(player->getRace()), player->GetName());
            return false;
        }
        desc.displayId = childDisplayId;
        LOG_DEBUG("module.dynamicquests.hungry_child",
            "HungryChild: using displayId={} for race={} player {}.",
            childDisplayId, static_cast<uint32>(player->getRace()), player->GetName());
    }

    TempSummon* courier = nullptr;
    if (isSuccubus)
    {
        // Succubus manages its own approach sequence via its phase state machine.
        // Spawn at the player's position; TickRitual teleports it to the correct
        // spot when its timer fires.
        courier = DQSpawnSystem::SpawnStationary(player, player->GetPosition(), desc);
    }
    else if (isHungryChild)
    {
        // Spawn child ~15y ahead of the player (in their field of view).
        // The child's AI (DQ_HungryChildAI) drives the walk toward the player —
        // no MoveFollow is set by DQSpawnSystem, so InitializeAI owns the motion.
        float spawnX = player->GetPositionX();
        float spawnY = player->GetPositionY();
        float spawnZ = player->GetPositionZ();
        player->GetNearPoint(nullptr, spawnX, spawnY, spawnZ, 1.0f, 15.0f,
            player->GetOrientation());
        Position spawnPos(spawnX, spawnY, spawnZ,
            player->GetOrientation() + static_cast<float>(M_PI));
        courier = DQSpawnSystem::SpawnStationary(player, spawnPos, desc);
    }
    else if (IsArchetypeId(templateId))
    {
        // Archetype courier: spawn style is read from the current beat's spawn_style column.
        uint32 archetypeId = DecodeArchetypeId(templateId);
        auto   it          = ps.archetypeProgress.find(archetypeId);
        uint8  beatNum     = (it != ps.archetypeProgress.end() && it->second != 0xFF)
                                ? it->second : 1;
        const ArchetypeBeat* beat  = sArchetypeMgr->GetBeat(archetypeId, beatNum);
        const std::string&   style = beat ? beat->spawnStyle : "approaches";
        courier = SpawnWithStyle(player, desc, style);
    }
    else
    {
        courier = DQSpawnSystem::SpawnCourier(player, desc);
    }

    if (!courier)
    {
        _phasePool.Release(phaseBit);
        LOG_DEBUG("module.dynamicquests",
            "SpawnCourier: failed for player {} (Z abort or summon failure).", player->GetName());
        return false;
    }

    // Apply phase to player so they see the normal world plus the private courier.
    if (phaseBit)
    {
        player->SetPhaseMask(player->GetPhaseMask() | phaseBit, true);
        ps.activePhaseBit = phaseBit;
    }
    else
    {
        ps.activePhaseBit = 0;
    }

    // Show ? marker — GetDialogStatus override in DQ_CourierScript returns the status.
    courier->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);

    ps.courierGuid      = courier->GetGUID();
    ps.activeTemplateId = templateId;
    ps.courierTimeoutMs = cfg_courierTimeoutMs;
    ps.lastCourierTheme = tmplPtr ? tmplPtr->theme : "";

    TransitionState(player, ps, DQ_STATE_INBOUND);

    LOG_INFO("module.dynamicquests", "Courier spawned: entry={} displayId={} template={} player={}",
        sel.entry, sel.displayId, templateId, player->GetName());

    if (ps.debugMode)
        ChatHandler(player->GetSession()).SendSysMessage(fmt::format(
            "[DQ+] Courier inbound. Template={} Entry={} DisplayId={}",
            templateId, sel.entry, sel.displayId));

    return true;
}

TempSummon* DynamicQuestMgr::SpawnWithStyle(Player* player, const DQSpawnDesc& desc,
    const std::string& style)
{
    // "approaches" — 22y ahead → MoveFollow (default)
    if (style.empty() || style == "approaches")
        return DQSpawnSystem::SpawnCourier(player, desc, 22.0f);

    // "distant" — 55y down the road; same MoveFollow arrival
    if (style == "distant")
        return DQSpawnSystem::SpawnCourier(player, desc, 55.0f);

    // "run_up" — sprints from 40y at 1.8× run speed
    if (style == "run_up")
    {
        TempSummon* c = DQSpawnSystem::SpawnCourier(player, desc, 40.0f);
        if (c)
            c->SetSpeed(MOVE_RUN, c->GetSpeed(MOVE_RUN) * 1.8f);
        return c;
    }

    // "roadside" — 25y ahead + 12y to the player's right; DQ_CourierAI starts MoveFollow
    if (style == "roadside")
    {
        float fwd = player->GetOrientation();
        float lat = fwd - float(M_PI) * 0.5f; // 90° clockwise = right side
        float px  = player->GetPositionX()
                    + 25.0f * std::cos(fwd)
                    + 12.0f * std::cos(lat);
        float py  = player->GetPositionY()
                    + 25.0f * std::sin(fwd)
                    + 12.0f * std::sin(lat);
        Position pos(px, py, player->GetPositionZ(), fwd + float(M_PI));
        return DQSpawnSystem::SpawnStationary(player, pos, desc);
    }

    // "waiting" / "collapses" — spawn within arrive-distance (2y) so courier transitions
    // on the first AI tick without visibly walking; emote_on_arrive fires via OnCourierArrived
    if (style == "waiting" || style == "collapses")
        return DQSpawnSystem::SpawnCourier(player, desc, 2.0f);

    // "from_portal" — summoning portal GO (entry 36727) appears at 6y; courier emerges from it
    if (style == "from_portal")
    {
        constexpr uint32 PORTAL_GO_ENTRY = 36727;
        constexpr uint32 PORTAL_DURATION = 8000;
        float ang = player->GetOrientation();
        float px  = player->GetPositionX() + 6.0f * std::cos(ang);
        float py  = player->GetPositionY() + 6.0f * std::sin(ang);
        Position portalPos(px, py, player->GetPositionZ(), 0.0f);
        DQSpawnSystem::SpawnGameObject(player, PORTAL_GO_ENTRY, portalPos,
            desc.phaseBit, PORTAL_DURATION);
        return DQSpawnSystem::SpawnCourier(player, desc, 6.0f);
    }

    // "from_shadow" — short approach (8y); feels like stepping from concealment
    if (style == "from_shadow")
        return DQSpawnSystem::SpawnCourier(player, desc, 8.0f);

    LOG_WARN("module.dynamicquests",
        "SpawnWithStyle: unknown style '{}' — falling back to 'approaches'.", style);
    return DQSpawnSystem::SpawnCourier(player, desc, 22.0f);
}

void DynamicQuestMgr::TransitionState(Player* player, PlayerDQState& ps, DQPlayerState newState)
{
    if (ps.state == newState)
        return;

    LOG_DEBUG("module.dynamicquests", "Player {} state: {} -> {}",
        player->GetName(), DQPlayerStateStr(ps.state), DQPlayerStateStr(newState));

    ps.state = newState;
}

uint32 DynamicQuestMgr::RollCooldown(uint8 tier) const
{
    uint32 minMs = (tier == 2) ? cfg_tier2MinMs : cfg_tier1MinMs;
    uint32 maxMs = (tier == 2) ? cfg_tier2MaxMs : cfg_tier1MaxMs;
    if (minMs >= maxMs)
        return minMs;

    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<uint32> dist(minMs, maxMs);
    return dist(rng);
}

void DynamicQuestMgr::AddToHistory(PlayerDQState& ps, uint32 templateId)
{
    if (templateId == 0)
        return;

    ps.history.push_front(templateId);
    while (ps.history.size() > cfg_historySize)
        ps.history.pop_back();
}

void DynamicQuestMgr::ApplyLoginData(ObjectGuid playerGuid, QueryResult result)
{
    // Callback runs on the world thread — safe to access game objects.
    Player* player = ObjectAccessor::FindPlayer(playerGuid);
    if (!player)
        return; // disconnected before the query came back

    PlayerDQState& ps = GetOrCreate(playerGuid);

    if (result)
    {
        Field* f = result->Fetch();
        ps.history.clear();
        for (int i = 0; i < 10; ++i)
        {
            uint32 id = f[i].Get<uint32>();
            if (id != 0)
                ps.history.push_back(id);
        }
        ps.lastCourierTheme = f[10].Get<std::string>();
        ps.lastEpisodeId    = f[11].Get<uint32>();
        uint32 nextTime     = f[12].Get<uint32>();
        ps.consecutiveTier1 = f[13].Get<uint8>();
        ps.ileanaEpisode    = f[14].Get<uint8>();
        ps.soulDebtUntil    = f[15].Get<uint32>();

        uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
        if (nextTime > now)
            ps.cooldownRemainMs = (nextTime - now) * 1000;
        else
            ps.cooldownRemainMs = RollCooldown(1);

        if (ps.soulDebtUntil > now)
        {
            int32 remainMs = static_cast<int32>((ps.soulDebtUntil - now) * 1000);
            if (Aura* debt = player->AddAura(15007, player))
                debt->SetDuration(remainMs);
        }
        else if (ps.soulDebtUntil != 0)
        {
            ps.soulDebtUntil = 0;
        }
    }

    LOG_DEBUG("module.dynamicquests", "Player {} DQ data applied. ileanaEp={} cooldown={}s",
        player->GetName(), ps.ileanaEpisode, ps.cooldownRemainMs / 1000);
}

void DynamicQuestMgr::ApplyArchetypeData(ObjectGuid playerGuid, QueryResult result)
{
    PlayerDQState& ps = GetOrCreate(playerGuid);
    ps.archetypeProgress.clear();

    if (result)
    {
        do
        {
            Field* f       = result->Fetch();
            uint32 arcId   = f[0].Get<uint32>();
            uint8  beat    = f[1].Get<uint8>();
            bool   done    = f[2].Get<uint8>() != 0;
            ps.archetypeProgress[arcId] = done ? 0xFF : beat;
        } while (result->NextRow());
    }

    // One-time migration: if ileana_episode > 0 and archetype_id=1 has no row yet,
    // seed character_dq_sequences from the legacy counter.
    // Only fires when the Ileana archetype (id=1) has been authored in dq_archetype.
    // Safe to run repeatedly — INSERT IGNORE is a no-op if the row already exists.
    if (ps.archetypeProgress.find(1) == ps.archetypeProgress.end() && ps.ileanaEpisode > 0)
    {
        bool   done = (ps.ileanaEpisode >= 3);
        uint8  beat = done ? 1 : (ps.ileanaEpisode + 1); // ep1=beat2, ep2=beat3, ep3=done
        uint32 now  = static_cast<uint32>(GameTime::GetGameTime().count());

        CharacterDatabase.Execute(
            "INSERT IGNORE INTO character_dq_sequences "
            "(guid, archetype_id, current_beat, beat_count, completed, last_seen) "
            "VALUES ({}, 1, {}, 0, {}, {})",
            playerGuid.GetCounter(), beat, done ? 1 : 0, now);

        ps.archetypeProgress[1] = done ? 0xFF : beat;

        LOG_DEBUG("module.dynamicquests",
            "Migrated ileana_episode={} → character_dq_sequences archetype_id=1 beat={}.",
            ps.ileanaEpisode, beat);
    }

    LOG_DEBUG("module.dynamicquests", "Archetype progress loaded: {} entries.",
        ps.archetypeProgress.size());
}

void DynamicQuestMgr::OnArchetypeBeatAdvanced(Player* player, uint32 archetypeId, uint8 beatOrCompleted)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ps.archetypeProgress[archetypeId] = beatOrCompleted;
}

uint8 DynamicQuestMgr::GetIleanaEpisode(Player* player) const
{
    const PlayerDQState* ps = Get(player->GetGUID());
    return ps ? ps->ileanaEpisode : 0;
}

void DynamicQuestMgr::SetSoulDebt(Player* player, uint32 expiryTimestamp)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ps.soulDebtUntil = expiryTimestamp;
}

void DynamicQuestMgr::ReleasePhase(Player* player, PlayerDQState& ps)
{
    if (!ps.activePhaseBit)
        return;

    uint32 bit    = ps.activePhaseBit;
    ps.activePhaseBit = 0; // clear first — idempotent if called twice

    if (player && player->IsInWorld())
    {
        uint32 newPhase = player->GetPhaseMask() & ~bit;
        player->SetPhaseMask(newPhase ? newPhase : 1, true); // never leave phase 0
    }

    _phasePool.Release(bit);
}
