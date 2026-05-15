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
#include "DQEmotionEngine.h"
#include "DQSpawnSystem.h"
#include "DynamicQuestMgr.h"
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
    const ArchetypeDef* def = sArchetypeMgr->Get(archetypeId);
    if (!def) return;

    uint8  currentBeat = inst.currentPhase;
    uint32 guid        = player->GetGUID().GetCounter();

    uint8 nextBeat  = currentBeat;
    bool  completed = false;

    switch (def->pattern)
    {
        // Counter = Sequential at the engine level; the "repeat N times" behaviour
        // is authored via encounter_count transition on beat 1 — no special case needed.
        case DQ_PATTERN_COUNTER:
        case DQ_PATTERN_SEQUENTIAL:
            nextBeat++;
            if (nextBeat > def->totalBeats)
                completed = true;
            break;

        case DQ_PATTERN_BRANCHING:
            // inst.choiceIndex set by DynamicQuestMgr::OnInteractionAccepted
            nextBeat = static_cast<uint8>(inst.choiceIndex) + 2;
            if (nextBeat > def->totalBeats)
                completed = true;
            break;
    }

    inst.completed = completed;
    // beat_count always resets to 0 when the beat advances.
    SaveBeatState(guid, archetypeId, nextBeat, 0, completed);

    // Update in-memory cache so TryTrigger immediately knows the new state.
    sDQMgr->OnArchetypeBeatAdvanced(player, archetypeId, completed ? 0xFF : nextBeat);

    LOG_INFO("module.dynamicquests",
        "ArchetypeEngine: {} archetype={} beat {}→{} completed={}",
        player->GetName(), archetypeId, currentBeat, nextBeat, completed ? 1 : 0);
}

void MechanicArchetype::CompleteBeat(Player* player, InteractionInstance& inst)
{
    uint32 archetypeId = DecodeArchetypeId(inst.templateId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst.currentPhase);

    if (beat)
    {
        Creature* courier = player->GetMap()->GetCreature(inst.courierGuid);

        // Legacy single-emote override (non-zero emoteOnComplete bypasses emotion system)
        if (beat->emoteOnComplete != 0 && courier)
            courier->HandleEmoteCommand(static_cast<Emote>(beat->emoteOnComplete));
        else if (courier)
        {
            // Resolve emotion_end → play first step of its onComplete sequence
            std::string resName = beat->emotionEnd.empty()
                ? sDQEmotions->GetDefaultResolution(beat->emotion)
                : beat->emotionEnd;
            if (!resName.empty())
                if (const DQEmotionDef* res = sDQEmotions->GetResolution(resName))
                    if (!res->onComplete.empty() && res->onComplete[0].emote != 0)
                        courier->HandleEmoteCommand(static_cast<Emote>(res->onComplete[0].emote));
        }

        // text_on_complete say (fires after resolution emote)
        if (!beat->textOnComplete.empty() && courier)
            courier->Say(beat->textOnComplete, LANG_UNIVERSAL);
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
        LOG_DEBUG("module.dynamicquests",
            "ArchetypeEngine: {} archetype={} already completed, aborting.",
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

    // Arrive emote
    if (beat->emoteOnArrive != 0)
        courier->HandleEmoteCommand(static_cast<uint32>(beat->emoteOnArrive));

    // Greeting text
    if (!beat->textGreeting.empty())
        courier->Say(beat->textGreeting, LANG_UNIVERSAL);

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
            break;
        }

        case DQ_TRANS_QUEST_COMPLETE:
            // Witness beats auto-complete on acknowledgement.
            // Courier/kill beats keep the player in ON_QUEST; OnKill/OnDelivery finish them.
            if (beat->mechanic == DQ_BEAT_WITNESS)
                CompleteBeat(player, inst);
            break;

        case DQ_TRANS_TIMER:
            // phaseTimer already set in OnStart; OnUpdate ticks it down.
            break;

        case DQ_TRANS_CHOICE:
            // inst.choiceIndex set by DynamicQuestMgr::OnInteractionAccepted before this call.
            CompleteBeat(player, inst);
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
    if (!inst.courierGuid.IsEmpty())
        if (Creature* courier = player->GetMap()->GetCreature(inst.courierGuid))
            courier->DespawnOrUnsummon(Milliseconds(2500)); // delay lets resolution emote play

    for (ObjectGuid guid : inst.auxGuidList)
        if (GameObject* go = player->GetMap()->GetGameObject(guid))
            go->Delete();
    inst.auxGuidList.clear();
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
