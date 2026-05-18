/*
 * DynamicQuests+ Module
 * Mechanic: Archetype — implementation
 *
 * Counter and Sequential are mechanically identical in AdvanceBeat.
 * The "repeat N times" Counter behaviour comes from encounter_count transition
 * in the DATA — no separate code path needed.
 *
 * character_dq_sequences.beat_count tracks:
 *   - encounter_count beats: visits completed for the current beat (reset on advance)
 *   - all other beats: unused (stays 0)
 */

#include "MechanicArchetype.h"
#include "ArchetypeMgr.h"
#include "DQAnimationMgr.h"
#include "DQDialogueMgr.h"
#include "DQEmotionEngine.h"
#include "DQNPCBuilder.h"
#include "DQSpawnSystem.h"
#include "DynamicQuestMgr.h"
#include "MechanicUtils.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "GameTime.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "SharedDefines.h"
#include <algorithm>
#include <cmath>
#include <random>

static constexpr uint32 DQ_CHOICE_FAIL_SENTINEL = 0xFE;

// ---------------------------------------------------------------------------
// DB helpers — character_dq_sequences
// ---------------------------------------------------------------------------

void MechanicArchetype::LoadBeatState(uint32 guid, uint32 archetypeId,
    uint8& outBeat, uint16& outCount, bool& outCompleted)
{
    outBeat      = 1;
    outCount     = 0;
    outCompleted = false;

    QueryResult result = CharacterDatabase.Query(
        "SELECT current_beat, beat_count, completed "
        "FROM character_dq_sequences WHERE guid={} AND archetype_id={}",
        guid, archetypeId);

    if (result)
    {
        Field* f     = result->Fetch();
        outBeat      = f[0].Get<uint8>();
        outCount     = f[1].Get<uint16>();
        outCompleted = f[2].Get<uint8>() != 0;
    }
}

void MechanicArchetype::SaveBeatState(uint32 guid, uint32 archetypeId,
    uint8 currentBeat, uint16 beatCount, bool completed)
{
    uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
    CharacterDatabase.Execute(
        "INSERT INTO character_dq_sequences "
        "(guid, archetype_id, current_beat, beat_count, completed, last_seen) "
        "VALUES ({},{},{},{},{},{}) "
        "ON DUPLICATE KEY UPDATE "
        "current_beat={}, beat_count={}, completed={}, last_seen={}",
        guid, archetypeId, currentBeat, beatCount, completed ? 1 : 0, now,
        currentBeat, beatCount, completed ? 1 : 0, now);
}

// ---------------------------------------------------------------------------
// Beat progression
// ---------------------------------------------------------------------------

void MechanicArchetype::AdvanceBeat(Player* player, InteractionInstance& inst)
{
    uint32 archetypeId = DecodeArchetypeId(inst.templateId);
    const ArchetypeDef*  def  = sArchetypeMgr->Get(archetypeId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst.currentPhase);
    if (!def) return;

    uint8  currentBeat = inst.currentPhase;
    uint32 guid        = player->GetGUID().GetCounter();

    bool  isFail    = (inst.choiceIndex == DQ_CHOICE_FAIL_SENTINEL);
    uint8 nextBeat  = currentBeat;
    bool  completed = false;

    // Phase 9: choice-aware routing via explicit transition columns.
    if (isFail && beat && beat->choiceFailTransition != 0)
    {
        nextBeat = beat->choiceFailTransition;
    }
    else if (!isFail && beat && beat->choiceSuccessTransition != 0)
    {
        nextBeat = beat->choiceSuccessTransition;
    }
    else
    {
        switch (def->pattern)
        {
            // Counter = Sequential at the engine level; the "repeat N times" behaviour
            // is authored via encounter_count transition on beat 1 -- no special case needed.
            case DQ_PATTERN_COUNTER:
            case DQ_PATTERN_SEQUENTIAL:
                nextBeat++;
                break;

            case DQ_PATTERN_BRANCHING:
                if (currentBeat == 1)
                    // Branch point: route to the chosen outcome beat
                    nextBeat = static_cast<uint8>(inst.choiceIndex) + 2;
                else
                    // Terminal outcome beat: complete the archetype
                    nextBeat = static_cast<uint8>(def->totalBeats) + 1;
                break;
        }
    }

    if (nextBeat > def->totalBeats)
        completed = true;

    // Phase 9: fire exit animation before the courier despawns.
    if (beat)
    {
        Creature* courier = player->GetMap()->GetCreature(inst.courierGuid);
        if (courier)
            sDQAnimation->PlayExit(courier, beat->exitAnimation, beat->exitSpell);
    }

    inst.completed = completed;
    // beat_count always resets to 0 when the beat advances.
    SaveBeatState(guid, archetypeId, nextBeat, 0, completed);

    // Update in-memory cache so TryTrigger immediately knows the new state.
    sDQMgr->OnArchetypeBeatAdvanced(player, archetypeId, completed ? 0xFF : nextBeat);

    LOG_INFO("module.dynamicquests",
        "ArchetypeEngine: {} archetype={} beat {}->{}  fail={} completed={}",
        player->GetName(), archetypeId, currentBeat, nextBeat, isFail ? 1 : 0, completed ? 1 : 0);
}

void MechanicArchetype::CompleteBeat(Player* player, InteractionInstance& inst)
{
    uint32 archetypeId = DecodeArchetypeId(inst.templateId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst.currentPhase);

    if (beat)
    {
        Creature* courier = player->GetMap()->GetCreature(inst.courierGuid);

        if (courier)
        {
            // emotion_end names the emotion whose opener fires on completion.
            // If empty, fall back to the beat's own emotion (same opener as greeting).
            const std::string& endName = beat->emotionEnd.empty() ? beat->emotion : beat->emotionEnd;
            if (!endName.empty())
                if (const DQEmotionDef* endDef = sDQEmotions->GetEmotion(endName))
                    if (endDef->openerEmote != 0)
                        courier->HandleEmoteCommand(static_cast<Emote>(endDef->openerEmote));
        }

        // text_on_complete say (fires after resolution emote)
        if (!beat->textOnComplete.empty() && courier)
            courier->Say(beat->textOnComplete, LANG_UNIVERSAL);

        // Deliver reward for this beat
        if (!inst.rewardPool.empty())
            DQMechanicUtils::DeliverReward(player, inst.rewardPool, player->GetLevel());
    }

    AdvanceBeat(player, inst);
    sDQMgr->OnInteractionComplete(player);
}

// ---------------------------------------------------------------------------
// IMechanicModule implementation
// ---------------------------------------------------------------------------

void MechanicArchetype::OnStart(Player* player, Creature* courier, InteractionInstance& inst)
{
    uint32 archetypeId = DecodeArchetypeId(inst.templateId);
    const ArchetypeDef* def = sArchetypeMgr->Get(archetypeId);
    if (!def || !courier)
        return;

    uint8  currentBeat;
    uint16 beatCount;
    bool   completed;
    LoadBeatState(player->GetGUID().GetCounter(), archetypeId,
        currentBeat, beatCount, completed);

    if (completed)
    {
        LOG_WARN("module.dynamicquests",
            "ArchetypeEngine: {} archetype={} already completed — aborting interaction. "
            "Clear character_dq_sequences for this archetype to reset.",
            player->GetName(), archetypeId);
        sDQMgr->AbortInteraction(player);
        return;
    }

    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, currentBeat);
    if (!beat)
    {
        LOG_ERROR("module.dynamicquests",
            "ArchetypeEngine: archetype={} beat={} not found in data.",
            archetypeId, currentBeat);
        return;
    }

    // Cache state in inst for use by OnChoice/OnUpdate/OnKill
    inst.currentPhase    = currentBeat;
    inst.objectiveCount  = beatCount;          // visits completed so far for this beat
    inst.objectiveTarget = beat->transitionValue; // target for encounter_count / kill
    inst.rewardPool      = beat->rewardPool;

    // Model override per beat (Descent archetype: each beat shows worsening state)
    if (beat->displayId != 0)
        courier->SetDisplayId(beat->displayId);

    // Greeting text
    if (!beat->textGreeting.empty())
        courier->Say(beat->textGreeting, LANG_UNIVERSAL);

    // Apply beat cost effects (drain gold and/or reduce HP)
    if (beat->costGoldPercent > 0)
    {
        uint32 total = player->GetMoney();
        uint32 drain = static_cast<uint32>(total * beat->costGoldPercent / 100.0f);
        if (drain > 0)
            player->ModifyMoney(-static_cast<int64>(drain));
    }
    if (beat->costHpPercent > 0)
    {
        uint32 newHp = static_cast<uint32>(player->GetMaxHealth() * beat->costHpPercent / 100.0f);
        newHp = std::max(1u, newHp);
        if (newHp < player->GetHealth())
            player->SetHealth(newHp);
    }

    // Phase 3: animation entry + persistent aura.
    sDQAnimation->PlayEntry(courier, beat->entryAnimation, beat->entrySpell);
    if (beat->auraSpell != 0)
        sDQAnimation->ApplyPersistentAura(courier, beat->auraSpell);

    // Cache fight/aura fields in inst so cleanup and concede handlers can use them.
    inst.concedePct = beat->fightThreshold;
    inst.auraSpell  = beat->auraSpell;

    // Pre-set timer for timer-transition beats
    if (beat->transitionType == DQ_TRANS_TIMER)
        inst.phaseTimer = beat->transitionValue * 1000u;

    // Scatter prop GOs for activate/gather beats
    if (beat->propEntry != 0 && beat->propCount > 0)
    {
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * float(M_PI));
        std::uniform_real_distribution<float> radDist(1.5f, beat->propRadius);

        for (uint8 i = 0; i < beat->propCount; ++i)
        {
            float a  = angleDist(rng);
            float r  = radDist(rng);
            float px = player->GetPositionX() + r * std::cos(a);
            float py = player->GetPositionY() + r * std::sin(a);
            Position pos(px, py, player->GetPositionZ(), 0.0f);
            if (GameObject* go = DQSpawnSystem::SpawnGameObject(
                    player, beat->propEntry, pos, inst.phaseBit, 600000))
                inst.auxGuidList.push_back(go->GetGUID());
        }
    }

    LOG_DEBUG("module.dynamicquests",
        "ArchetypeEngine: {} archetype={} beat={} mechanic={} transition={} count={}/{}",
        player->GetName(), archetypeId, currentBeat,
        static_cast<int>(beat->mechanic),
        static_cast<int>(beat->transitionType),
        beatCount, beat->transitionValue);
}

void MechanicArchetype::OnChoice(Player* player, uint32 /*choiceId*/, InteractionInstance& inst)
{
    uint32 archetypeId = DecodeArchetypeId(inst.templateId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst.currentPhase);
    if (!beat) return;

    // Transition-type guard (encounter_count, timer, and CHOICE transitions are handled
    // above the mechanic switch -- they override the mechanic dispatch for those beat types).
    switch (beat->transitionType)
    {
        case DQ_TRANS_ENCOUNTER_COUNT:
        {
            uint16 newCount = static_cast<uint16>(inst.objectiveCount) + 1;
            inst.objectiveCount = newCount;
            if (newCount >= inst.objectiveTarget)
            {
                CompleteBeat(player, inst);
            }
            else
            {
                // Target not reached: record visit, return to cooldown; same beat fires next time.
                SaveBeatState(player->GetGUID().GetCounter(), archetypeId,
                    inst.currentPhase, newCount, false);
                sDQMgr->OnInteractionComplete(player);
            }
            return;
        }

        case DQ_TRANS_TIMER:
            // phaseTimer already set in OnStart; OnUpdate ticks it down.  Nothing to do here.
            return;

        case DQ_TRANS_CHOICE:
            // inst.choiceIndex set by DynamicQuestMgr::OnInteractionAccepted before this call.
            CompleteBeat(player, inst);
            return;

        case DQ_TRANS_QUEST_COMPLETE:
            break; // fall through to mechanic dispatch below
    }

    // Phase 7: mechanic dispatch for QUEST_COMPLETE transition beats.
    switch (beat->mechanic)
    {
        case DQ_BEAT_WITNESS:
            CompleteBeat(player, inst);
            break;

        case DQ_BEAT_COURIER:
            BeginCourierObjective(player, inst, beat);
            break;

        case DQ_BEAT_KILL:
            BeginKillObjective(player, inst, beat);
            break;

        case DQ_BEAT_GOTO:
            BeginGotoObjective(player, inst, beat);
            break;

        case DQ_BEAT_ACTIVATE:
            // Props already scattered in OnStart; player must click them.
            // OnActivate fires when they do -- no additional setup needed here.
            break;

        case DQ_BEAT_FIGHT:
            BeginFight(player, inst, beat);
            break;

        case DQ_BEAT_CAST:
            RegisterPassiveCast(player, inst, beat);
            break;
    }
}

void MechanicArchetype::OnComplete(Player* /*player*/, InteractionInstance& /*inst*/)
{
    // Completion is driven by CompleteBeat() → AdvanceBeat() → OnInteractionComplete().
    // This override exists only for interface compliance.
}

void MechanicArchetype::OnFail(Player* player, InteractionInstance& inst)
{
    LOG_DEBUG("module.dynamicquests",
        "ArchetypeEngine: {} archetype={} beat={} failed. Beat state preserved.",
        player->GetName(), DecodeArchetypeId(inst.templateId), inst.currentPhase);
}

void MechanicArchetype::OnCleanup(Player* player, InteractionInstance& inst)
{
    Creature* courier = inst.courierGuid.IsEmpty()
        ? nullptr
        : player->GetMap()->GetCreature(inst.courierGuid);

    // Phase 9: remove persistent aura before despawn.
    if (courier && inst.auraSpell != 0)
        sDQAnimation->RemovePersistentAura(courier, inst.auraSpell);

    if (courier)
    {
        uint32 despawnMs = 5000;
        uint32 archetypeId = DecodeArchetypeId(inst.templateId);
        const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst.currentPhase);
        if (beat && !beat->emotion.empty())
        {
            const std::string& endName = beat->emotionEnd.empty() ? beat->emotion : beat->emotionEnd;
            if (const DQEmotionDef* endDef = sDQEmotions->GetEmotion(endName))
                if (endDef->openerEmote != 0)
                    despawnMs = DQEmotionEngine::GetEmoteDuration(endDef->openerEmote) + 3000;
        }
        courier->DespawnOrUnsummon(Milliseconds(despawnMs));
    }

    for (ObjectGuid guid : inst.auxGuidList)
        if (GameObject* go = player->GetMap()->GetGameObject(guid))
            go->Delete();
    inst.auxGuidList.clear();

    // Phase 9: remove from passive-hook reverse map (safe even if never registered).
    sDQMgr->UnregisterActiveCourier(inst.courierGuid);
}

void MechanicArchetype::OnActivate(Player* player, GameObject* go, InteractionInstance& inst)
{
    // Only accept clicks on GOs that belong to this interaction
    auto it = std::find(inst.auxGuidList.begin(), inst.auxGuidList.end(), go->GetGUID());
    if (it == inst.auxGuidList.end())
        return;

    inst.auxGuidList.erase(it);
    go->Delete();

    inst.objectiveCount++;
    if (inst.objectiveCount >= inst.objectiveTarget)
        CompleteBeat(player, inst);
}

void MechanicArchetype::OnUpdate(Player* player, uint32 diff, InteractionInstance& inst)
{
    uint32 archetypeId = DecodeArchetypeId(inst.templateId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst.currentPhase);
    if (!beat || beat->transitionType != DQ_TRANS_TIMER)
        return;

    if (inst.phaseTimer <= diff)
    {
        inst.phaseTimer = 0;

        // If the beat has an explicit fail transition, timer expiry routes through it.
        if (beat->choiceFailTransition != 0)
            inst.choiceIndex = DQ_CHOICE_FAIL_SENTINEL;

        CompleteBeat(player, inst);
    }
    else
    {
        inst.phaseTimer -= diff;
    }
}

void MechanicArchetype::OnKill(Player* player, Creature* victim, InteractionInstance& inst)
{
    uint32 archetypeId = DecodeArchetypeId(inst.templateId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst.currentPhase);
    if (!beat || beat->mechanic != DQ_BEAT_KILL) return;
    if (beat->transitionType != DQ_TRANS_QUEST_COMPLETE) return;

    // objectiveEntry 0 = any kill counts; otherwise match creature entry
    if (inst.objectiveEntry != 0 && victim && victim->GetEntry() != inst.objectiveEntry)
        return;

    inst.objectiveCount++;
    if (inst.objectiveCount >= inst.objectiveTarget)
        CompleteBeat(player, inst);
}

void MechanicArchetype::ForceAdvance(Player* player, InteractionInstance& inst)
{
    LOG_INFO("module.dynamicquests",
        "ArchetypeEngine: ForceAdvance {} archetype={} beat={}",
        player->GetName(), DecodeArchetypeId(inst.templateId), inst.currentPhase);
    CompleteBeat(player, inst);
}

// ---------------------------------------------------------------------------
// Phase 7 — action router helpers
// ---------------------------------------------------------------------------

void MechanicArchetype::BeginCourierObjective(Player* /*player*/, InteractionInstance& inst,
    const ArchetypeBeat* beat)
{
    // Item prereq already consumed by DQ_CourierAI HandleGossipSelect.
    // Mark objective as active; OnDelivery or passive hook completes it.
    inst.objectiveTarget = 1;
    inst.objectiveCount  = 0;
    (void)beat;
}

void MechanicArchetype::BeginKillObjective(Player* /*player*/, InteractionInstance& inst,
    const ArchetypeBeat* beat)
{
    // objectiveEntry 0 = any kill counts (kill_entry column deferred to Phase 11).
    inst.objectiveEntry  = 0;
    inst.objectiveTarget = beat->transitionValue;
    inst.objectiveCount  = 0;
}

void MechanicArchetype::BeginGotoObjective(Player* /*player*/, InteractionInstance& inst,
    const ArchetypeBeat* /*beat*/)
{
    // Zone-based completion wired in Phase 10 via OnPlayerZoneChange.
    // For now: set a sentinel so OnPlayerZoneChange can identify an active GOTO beat.
    inst.objectiveEntry  = 0xFFFFFFFF;
    inst.objectiveTarget = 1;
    inst.objectiveCount  = 0;
}

void MechanicArchetype::RegisterPassiveCast(Player* /*player*/, InteractionInstance& /*inst*/,
    const ArchetypeBeat* /*beat*/)
{
    // Passive CAST: DQ_SpellCastHook (DQ_PassiveHooks.cpp) is already listening globally.
    // No additional registration needed; the hook checks state==ON_QUEST and mechanic==CAST.
}

// ---------------------------------------------------------------------------
// Phase 8 — fight handler
// ---------------------------------------------------------------------------

void MechanicArchetype::BeginFight(Player* player, InteractionInstance& inst,
    const ArchetypeBeat* beat)
{
    Creature* courier = player->GetMap()->GetCreature(inst.courierGuid);
    if (!courier)
        return;

    DQNPCBuilder::FlipToHostile(courier, player);
    sDQAnimation->PlayBeatTransition(courier, player);
    inst.concedePct = beat->fightThreshold;
}

void MechanicArchetype::OnFightConcede(Player* player, InteractionInstance& inst)
{
    Creature* courier = player->GetMap()->GetCreature(inst.courierGuid);
    if (courier)
    {
        DQNPCBuilder::FlipToFriendly(courier);

        // Say a concede line from the text variant pool if available.
        uint32 archetypeId = DecodeArchetypeId(inst.templateId);
        const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst.currentPhase);
        if (beat && !beat->emotion.empty())
        {
            std::string line = sDQDialogue->GetVariantText(beat->emotion, "", "concede");
            if (!line.empty())
                courier->Say(line, LANG_UNIVERSAL);
        }
    }

    CompleteBeat(player, inst);
}
