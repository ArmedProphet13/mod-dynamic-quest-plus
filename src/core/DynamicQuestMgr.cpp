/*
 * DynamicQuests+ Module
 * Central Manager — Implementation
 */

#include "DynamicQuestMgr.h"
#include "DQClientSession.h"
#include "DQDialogueMgr.h"
#include "DQSpawnSystem.h"
#include "WorldCatalogue.h"
#include "MechanicArchetype.h"
#include "ArchetypeMgr.h"
#include "DQEmotionEngine.h"
#include "SpellInfo.h"
#include "Spell.h"
#include "Chat.h"
#include "Config.h"
#include "Field.h"
#include "QueryResult.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GameTime.h"
#include "DQLog.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "WorldSession.h"
#include "TemporarySummon.h"
#include <cmath>
#include <random>
#include <sstream>
#include <fmt/format.h>

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
    sArchetypeMgr->LoadFromDB();
    sDQDialogue->LoadFromDB();
    RegisterMechanics();

    DQ_LOG_INFO(DQ_LOG_CAT_STATE, nullptr, "Initialized. CatalogueEntries={} Archetypes={} Mechanics={}",
        sWorldCatalogue->GetEntryCount(), sArchetypeMgr->GetCount(), _mechanics.size());
}

void DynamicQuestMgr::RegisterMechanics()
{
    static constexpr uint8 MTYPE_ARCHETYPE = 9;
    _mechanics[MTYPE_ARCHETYPE] = std::make_unique<MechanicArchetype>();
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
}

void DynamicQuestMgr::ReloadTemplates()
{
    sWorldCatalogue->LoadFromDB();
    sArchetypeMgr->LoadFromDB();
    DQ_LOG_INFO(DQ_LOG_CAT_STATE, nullptr, "Reloaded. CatalogueEntries={} Archetypes={}",
        sWorldCatalogue->GetEntryCount(), sArchetypeMgr->GetCount());
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

        case DQ_STATE_DELIVERING:
            // Tick the gossip session expiry. On expiry, abort (player ignored the courier).
            if (DQClientSession::Tick(ps.pendingSession, diff))
            {
                DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "Gossip session expired. Aborting.");
                AbortInteraction(player);
            }
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
        "last_courier_theme,last_episode_id,next_trigger_time,consecutive_tier1) "
        "VALUES ({},{},{},{},{},{},{},{},{},{},{},'{}',0,{},{}) "
        "ON DUPLICATE KEY UPDATE "
        "last_template_1={},last_template_2={},last_template_3={},last_template_4={},"
        "last_template_5={},last_template_6={},last_template_7={},last_template_8={},"
        "last_template_9={},last_template_10={},"
        "last_courier_theme='{}',next_trigger_time={},consecutive_tier1={}",
        player->GetGUID().GetCounter(),
        h[0],h[1],h[2],h[3],h[4],h[5],h[6],h[7],h[8],h[9],
        ps.lastCourierTheme, nextTrigger, ps.consecutiveTier1,
        h[0],h[1],h[2],h[3],h[4],h[5],h[6],h[7],h[8],h[9],
        ps.lastCourierTheme, nextTrigger, ps.consecutiveTier1);

    _states.erase(player->GetGUID().GetRawValue());

    DQ_LOG_DEBUG(DQ_LOG_CAT_STATE, player, "Logged out. DQ state saved.");
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
        "last_courier_theme,next_trigger_time,consecutive_tier1 "
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

    DQ_LOG_DEBUG(DQ_LOG_CAT_STATE, player, "Logged in. DQ data loading async.");
}

// ---------------------------------------------------------------------------
// Courier events
// ---------------------------------------------------------------------------

void DynamicQuestMgr::OnCourierArrived(Player* player, Creature* courier)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    if (ps.state != DQ_STATE_INBOUND)
        return;

    // Preserve portal/aux GUIDs placed by SpawnCourier so OnCleanup can delete them.
    auto savedAuxGuids = std::move(ps.activeInst.auxGuidList);

    ps.activeInst = {};
    ps.activeInst.templateId  = ps.activeTemplateId;
    ps.activeInst.courierGuid = courier ? courier->GetGUID() : ps.courierGuid;
    ps.activeInst.phaseBit    = ps.activePhaseBit; // propagate to mechanics for private spawns
    ps.activeInst.auxGuidList = std::move(savedAuxGuids);

    TransitionState(player, ps, DQ_STATE_DELIVERING);

    if (IMechanicModule* m = GetMechanic(ps.activeTemplateId))
        m->OnStart(player, courier, ps.activeInst);

    DQ_LOG_INFO(DQ_LOG_CAT_COURIER, player, "Courier arrived. Template={}", ps.activeTemplateId);
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

    DQ_LOG_INFO(DQ_LOG_CAT_SESSION, player, "Interaction accepted. template={} choice={}", templateId, choiceIdx);
}

void DynamicQuestMgr::OnInteractionDeclined(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ReleasePhase(player, ps);
    TransitionState(player, ps, DQ_STATE_COOLDOWN);
    ps.cooldownRemainMs = RollCooldown(1);
    DQ_LOG_INFO(DQ_LOG_CAT_SESSION, player, "Interaction declined. Cooldown={}s", ps.cooldownRemainMs / 1000);
}

void DynamicQuestMgr::OnInteractionComplete(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ReleasePhase(player, ps);

    // Archetypes: only add to history when the full arc is done.
    if (IsArchetypeId(ps.activeTemplateId) && ps.activeInst.completed)
        AddToHistory(ps, ps.activeTemplateId);

    ps.consecutiveTier1++;

    ps.activeTemplateId = 0;
    ps.activeQuestId    = 0;

    TransitionState(player, ps, DQ_STATE_COOLDOWN);
    ps.cooldownRemainMs = RollCooldown(1);

    DQ_LOG_INFO(DQ_LOG_CAT_STATE, player, "Interaction completed. cooldown={}s", ps.cooldownRemainMs / 1000);
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

        // Archetypes — pick eligible one by level/zone/tags
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
            DQ_LOG_ERROR(DQ_LOG_CAT_STATE, player, "ForceTriggger: No template found (level={}, zone={}).", playerLevel, playerZone);
            return;
        }
    }

    // Archetype spawn path
    if (IsArchetypeId(selectedTemplate))
    {
        uint32 archetypeId = DecodeArchetypeId(selectedTemplate);
        CourierSelection sel;
        if (!SelectCourier(archetypeId, ps, sel))
        {
            DQ_LOG_WARN(DQ_LOG_CAT_SPAWN, player, "ForceTriggger: No social courier entry for archetype {}.", archetypeId);
            return;
        }
        SpawnCourier(player, ps, selectedTemplate, sel);
    }
}

void DynamicQuestMgr::AbortInteraction(Player* player)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    DQClientSession::Close(ps.pendingSession);
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
    ps.cooldownRemainMs = RollCooldown(1);
    TransitionState(player, ps, DQ_STATE_COOLDOWN);
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
    ps.consecutiveTier1 = 0;
    CharacterDatabase.Execute("DELETE FROM character_dq_history WHERE guid = {}",
        player->GetGUID().GetCounter());
    DQ_LOG_DEBUG(DQ_LOG_CAT_STATE, player, "History cleared.");
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

DQPendingSession& DynamicQuestMgr::PendingSession(Player* player)
{
    return GetOrCreate(player->GetGUID()).pendingSession;
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
    if (!IsArchetypeId(templateId))
        return nullptr;
    static constexpr uint8 MTYPE_ARCHETYPE = 9;
    auto it = _mechanics.find(MTYPE_ARCHETYPE);
    return it != _mechanics.end() ? it->second.get() : nullptr;
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
        DQ_LOG_DEBUG(DQ_LOG_CAT_COURIER, player, "Courier timeout. Aborting.");
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

    // Build pool from eligible archetypes.
    // Completed archetypes are excluded; progression tracked in character_dq_sequences.
    std::vector<std::pair<uint32, int32>> pool; // (encoded templateId, weight)
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
    static std::mt19937 rng{ std::random_device{}() };
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

    uint32 archetypeId = DecodeArchetypeId(selectedId);
    CourierSelection sel;
    if (!SelectCourier(archetypeId, ps, sel))
    {
        DQ_LOG_WARN(DQ_LOG_CAT_SPAWN, player, "TryTrigger: No social courier entry for archetype {}. Skipping.", archetypeId);
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
        DQ_LOG_WARN(DQ_LOG_CAT_SPAWN, player, "Phase pool exhausted — courier is world-visible.");

    DQSpawnDesc desc;
    desc.entry      = sel.entry;
    desc.phaseBit   = phaseBit;
    desc.displayId  = sel.displayId;
    desc.scaleLevel = true;

    // Set activeTemplateId AND pre-init activeInst BEFORE SummonCreature so
    // DQ_CourierAI::InitializeAI can read templateId + currentPhase to resolve
    // the beat's emotion definition.
    ps.activeTemplateId = templateId;

    // Spawn style is read from the current beat's spawn_style column.
    uint32 archetypeId = DecodeArchetypeId(templateId);
    auto   it          = ps.archetypeProgress.find(archetypeId);
    uint8  beatNum     = (it != ps.archetypeProgress.end() && it->second != 0xFF)
                            ? it->second : 1;

    // Pre-seed activeInst so InitializeAI sees a valid templateId and beat number.
    // OnCourierArrived will re-initialise this fully; we just need the identity fields.
    ps.activeInst.templateId   = templateId;
    ps.activeInst.currentPhase = beatNum;

    const ArchetypeBeat* beat  = sArchetypeMgr->GetBeat(archetypeId, beatNum);
    const std::string&   style = beat ? beat->spawnStyle : "approaches";
    TempSummon* courier = DQSpawnSystem::SpawnWithStyle(player, desc, style,
        &ps.activeInst.auxGuidList);

    if (!courier)
    {
        _phasePool.Release(phaseBit);
        DQ_LOG_DEBUG(DQ_LOG_CAT_SPAWN, player, "SpawnCourier failed (Z abort or summon failure).");
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

    ps.courierGuid      = courier->GetGUID();
    ps.courierTimeoutMs = cfg_courierTimeoutMs;
    RegisterActiveCourier(courier->GetGUID(), player->GetGUID());
    if (const ArchetypeDef* def = sArchetypeMgr->Get(DecodeArchetypeId(templateId)))
        ps.lastCourierTheme = def->appearance;   // appearance is SQL-safe; name may contain apostrophes
    else
        ps.lastCourierTheme.clear();

    TransitionState(player, ps, DQ_STATE_INBOUND);

    DQ_LOG_INFO(DQ_LOG_CAT_SPAWN, player, "Courier spawned: entry={} displayId={} template={}",
        sel.entry, sel.displayId, templateId);

    if (ps.debugMode)
        ChatHandler(player->GetSession()).SendSysMessage(fmt::format(
            "[DQ+] Courier inbound. Template={} Entry={} DisplayId={}",
            templateId, sel.entry, sel.displayId));

    return true;
}

bool DynamicQuestMgr::SelectCourier(uint32 archetypeId,
    const PlayerDQState& ps, CourierSelection& out) const
{
    out.entry = sWorldCatalogue->GetCourierEntryForTheme("social");
    if (out.entry == 0)
        return false;
    if (const ArchetypeDef* def = sArchetypeMgr->Get(archetypeId))
        if (!def->appearance.empty())
            out.displayId = sWorldCatalogue->GetWorldCourierDisplayId(
                ps.ctx.zoneId, SplitTags(def->appearance), {}, 1);
    return true;
}

void DynamicQuestMgr::TransitionState(Player* player, PlayerDQState& ps, DQPlayerState newState)
{
    if (ps.state == newState)
        return;

    DQ_LOG_DEBUG(DQ_LOG_CAT_STATE, player, "State: {} -> {}", DQPlayerStateStr(ps.state), DQPlayerStateStr(newState));

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
        uint32 nextTime     = f[11].Get<uint32>();
        ps.consecutiveTier1 = f[12].Get<uint8>();

        uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
        if (nextTime > now)
            ps.cooldownRemainMs = (nextTime - now) * 1000;
        else
            ps.cooldownRemainMs = RollCooldown(1);
    }

    DQ_LOG_DEBUG(DQ_LOG_CAT_STATE, player, "DQ data applied. cooldown={}s", ps.cooldownRemainMs / 1000);
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

    DQ_LOG_DEBUG(DQ_LOG_CAT_STATE, nullptr, "Archetype progress loaded: {} entries.", ps.archetypeProgress.size());
}

// ---------------------------------------------------------------------------
// Passive mechanic support
// ---------------------------------------------------------------------------

void DynamicQuestMgr::RegisterActiveCourier(ObjectGuid courier, ObjectGuid player)
{
    _courierToPlayer[courier.GetRawValue()] = player;
}

void DynamicQuestMgr::UnregisterActiveCourier(ObjectGuid courier)
{
    _courierToPlayer.erase(courier.GetRawValue());
}

uint8 DynamicQuestMgr::GetCurrentBeat(Player* player) const
{
    const PlayerDQState* ps = Get(player->GetGUID());
    return ps ? ps->activeInst.currentPhase : 0;
}

void DynamicQuestMgr::OnCourierHealed(ObjectGuid courierGuid, Player* healer, uint32 /*amount*/)
{
    auto it = _courierToPlayer.find(courierGuid.GetRawValue());
    if (it == _courierToPlayer.end())
        return;

    if (it->second != healer->GetGUID())
        return;

    PlayerDQState* ps = &GetOrCreate(healer->GetGUID());
    if (ps->state != DQ_STATE_ON_QUEST)
        return;

    uint32 archetypeId = DecodeArchetypeId(ps->activeTemplateId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, ps->activeInst.currentPhase);
    if (!beat || beat->mechanic != DQ_BEAT_CAST)
        return;

    if (IMechanicModule* m = GetMechanic(ps->activeTemplateId))
        m->OnComplete(healer, ps->activeInst);
}

void DynamicQuestMgr::OnCourierTookDamage(ObjectGuid courierGuid, uint32 remainingHealthPct)
{
    auto it = _courierToPlayer.find(courierGuid.GetRawValue());
    if (it == _courierToPlayer.end())
        return;

    Player* player = ObjectAccessor::FindPlayer(it->second);
    if (!player)
        return;

    PlayerDQState* ps = &GetOrCreate(it->second);
    if (ps->state != DQ_STATE_ON_QUEST)
        return;

    uint32 archetypeId = DecodeArchetypeId(ps->activeTemplateId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, ps->activeInst.currentPhase);
    if (!beat || beat->mechanic != DQ_BEAT_FIGHT)
        return;

    if (remainingHealthPct <= ps->activeInst.concedePct)
        OnCourierConceded(player);
}

void DynamicQuestMgr::OnPlayerCastOnCourier(Player* player, Spell* spell)
{
    PlayerDQState* ps = &GetOrCreate(player->GetGUID());
    if (ps->state != DQ_STATE_ON_QUEST)
        return;

    uint32 archetypeId = DecodeArchetypeId(ps->activeTemplateId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, ps->activeInst.currentPhase);
    if (!beat || beat->mechanic != DQ_BEAT_CAST)
        return;

    // Check the spell's explicit unit target matches our courier.
    Unit* target = spell->m_targets.GetUnitTarget();
    if (!target || target->GetGUID() != ps->activeInst.courierGuid)
        return;

    // School check: castSchool == 0 means any school is accepted.
    if (beat->castSchool != 0)
    {
        SpellInfo const* spellInfo = spell->GetSpellInfo();
        if (!spellInfo || !(spellInfo->GetSchoolMask() & (1u << beat->castSchool)))
            return;
    }

    if (IMechanicModule* m = GetMechanic(ps->activeTemplateId))
        m->OnComplete(player, ps->activeInst);
}

void DynamicQuestMgr::OnCourierConceded(Player* player)
{
    PlayerDQState* ps = &GetOrCreate(player->GetGUID());
    if (ps->state != DQ_STATE_ON_QUEST)
        return;

    IMechanicModule* m = GetMechanic(ps->activeTemplateId);
    if (m)
        m->OnFightConcede(player, ps->activeInst);
}

void DynamicQuestMgr::OnArchetypeBeatAdvanced(Player* player, uint32 archetypeId, uint8 beatOrCompleted)
{
    PlayerDQState& ps = GetOrCreate(player->GetGUID());
    ps.archetypeProgress[archetypeId] = beatOrCompleted;
}

void DynamicQuestMgr::ReleasePhase(Player* player, PlayerDQState& ps)
{
    // Remove passive-hook reverse lookup entry before clearing courierGuid.
    if (!ps.courierGuid.IsEmpty())
        UnregisterActiveCourier(ps.courierGuid);

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
