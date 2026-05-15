/*
 * DynamicQuests+ Module
 * Central Manager / Singleton
 *
 * Owns all per-player state (CourierState), coordinates the six systems,
 * and exposes the public API used by scripts and GM commands.
 */

#ifndef DQ_DYNAMIC_QUEST_MGR_H
#define DQ_DYNAMIC_QUEST_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include "QueryResult.h"
#include "Position.h"
#include "PlayerContextObserver.h"
#include "EligibilityEngine.h"
#include "IMechanicModule.h"
#include "NPCMatchingEngine.h"
#include "DQSpawnSystem.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>
#include <deque>

// ---------------------------------------------------------------------------
// DQPhasePool — lock-free private-phase slot allocator
//
// WoW WotLK phase mask is a uint32 bitmask. Bit 0 (value 1) is the normal
// world; bits 1-30 are free for custom use. Bit 31 is skipped (sign-bit risk
// with signed comparisons elsewhere in the engine).
//
// A courier spawned with phaseMask=bit is invisible to all players whose
// phaseMask doesn't share that bit. The interaction player gets the bit added
// to their mask for the duration, then it's stripped on cleanup.
//
// Acquire/Release are O(1) CAS operations — no mutex, no heap allocation.
// Pool exhaustion (>30 concurrent active interactions) falls back gracefully
// to world-visible courier rather than blocking or crashing.
// ---------------------------------------------------------------------------
class DQPhasePool
{
public:
    static constexpr uint32 POOL_MASK = 0x7FFFFFFEu; // bits 1-30

    // Returns a single power-of-2 bit from POOL_MASK, or 0 if exhausted.
    uint32 Acquire()
    {
        uint32 current, next, bit;
        do
        {
            current = _used.load(std::memory_order_relaxed);
            uint32 free = ~current & POOL_MASK;
            if (!free)
                return 0;
            bit  = free & (0u - free); // isolate lowest free bit, O(1)
            next = current | bit;
        }
        while (!_used.compare_exchange_weak(current, next,
                std::memory_order_acquire, std::memory_order_relaxed));
        return bit;
    }

    void Release(uint32 bit)
    {
        if (bit)
            _used.fetch_and(~bit, std::memory_order_release);
    }

private:
    std::atomic<uint32> _used{ 0 };
};

enum DQPlayerState : uint8
{
    DQ_STATE_IDLE       = 0, // waiting for cooldown
    DQ_STATE_SCORING    = 1, // evaluating eligibility (transient, one tick)
    DQ_STATE_INBOUND    = 2, // courier spawned and running to player
    DQ_STATE_DELIVERING = 3, // courier reached player, gossip open
    DQ_STATE_ON_QUEST   = 4, // player accepted, mechanic running
    DQ_STATE_COOLDOWN   = 5, // interaction complete, counting down
};

inline const char* DQPlayerStateStr(DQPlayerState s)
{
    switch (s)
    {
        case DQ_STATE_IDLE:       return "IDLE";
        case DQ_STATE_SCORING:    return "SCORING";
        case DQ_STATE_INBOUND:    return "INBOUND";
        case DQ_STATE_DELIVERING: return "DELIVERING";
        case DQ_STATE_ON_QUEST:   return "ON_QUEST";
        case DQ_STATE_COOLDOWN:   return "COOLDOWN";
        default:                  return "UNKNOWN";
    }
}

struct PlayerDQState
{
    DQPlayerState   state           = DQ_STATE_IDLE;
    PlayerContext   ctx             = {};

    // Cooldown
    uint32  cooldownRemainMs    = 0;
    uint32  tier1CooldownMs     = 0;
    uint32  tier2CooldownMs     = 0;

    // Active courier tracking
    ObjectGuid  courierGuid     = ObjectGuid::Empty;
    uint32  courierTimeoutMs    = 0; // ms until courier gives up
    uint32  activeTemplateId    = 0;
    uint32  activeQuestId       = 0; // 0 if gossip-only interaction
    uint32  activePhaseBit      = 0; // private phase slot; 0 = world-visible fallback

    // Consecutive tier 1 counter for escalation
    uint8   consecutiveTier1    = 0;

    // Per-player debug mode (toggled by .dq debug on/off)
    bool    debugMode           = false;

    // Quest history: last N quest/template IDs
    std::deque<uint32> history; // front = most recent
    std::string lastCourierTheme;
    uint32  lastEpisodeId       = 0;

    // Active mechanic instance
    InteractionInstance activeInst;

    // Ileana encounter progress (0 = not started, 1 = ep1 done, 2 = ep2 done, 3 = all done)
    uint8  ileanaEpisode  = 0;
    // Unix timestamp after which the soul-debt aura (ep3) has expired; 0 = none
    uint32 soulDebtUntil  = 0;

    // Internal: used for .dq status diagnostic output
    EligibilityResult lastGateResult = {};

    // Per-archetype progress cache (loaded async at login, updated on beat completion).
    // Key: archetype_id. Value: current_beat, or 0xFF when the archetype is completed.
    // Absent entry means the player has never encountered this archetype (treat as beat 1).
    std::unordered_map<uint32, uint8> archetypeProgress;
};

class Player;
class Creature;
class GameObject;
class TempSummon;

class DynamicQuestMgr
{
public:
    static DynamicQuestMgr* instance();

    // Called by DQ_WorldScript::OnStartup — loads all systems
    void Initialize();

    // Called by DQ_WorldScript on config reload
    void LoadConfig();

    // Called by .dq reload command
    void ReloadTemplates();

    // --- Per-player lifecycle (called from DQ_PlayerScript) ---

    void OnPlayerUpdate(Player* player, uint32 diff);
    void OnPlayerZoneChange(Player* player, uint32 newZone, uint32 newArea);
    void OnPlayerKill(Player* player, Creature* victim);
    void OnPlayerLoot(Player* player, uint32 itemId);
    void OnPlayerLogout(Player* player);
    void OnPlayerLogin(Player* player);

    // --- Courier events (called from DQ_CourierAI) ---

    // Courier reached player and stopped. Opens gossip and transitions to DELIVERING.
    void OnCourierArrived(Player* player, Creature* courier);

    // Player accepted the interaction from gossip. Transitions to ON_QUEST.
    void OnInteractionAccepted(Player* player, uint32 templateId, uint32 choiceIdx);

    // Player dismissed gossip without accepting. Cleans up courier, resets to COOLDOWN.
    void OnInteractionDeclined(Player* player);

    // Called by MechanicModules on completion/failure.
    void OnInteractionComplete(Player* player);
    void OnInteractionFailed(Player* player);

    // Called by MechanicArchetype when a beat advances.
    // beatOrCompleted: next beat number, or 0xFF when the archetype is fully done.
    void OnArchetypeBeatAdvanced(Player* player, uint32 archetypeId, uint8 beatOrCompleted);

    // --- GM Command API ---

    // Force trigger a courier for a player, bypassing eligibility.
    // If templateId=0, selects the best match normally.
    void ForceTriggger(Player* player, uint32 templateId = 0);

    // Abort current interaction and clean up all spawns.
    void AbortInteraction(Player* player);

    // Returns a formatted status string for .dq status
    std::string GetStatusString(Player* player) const;

    // Returns a formatted gate check string for .dq debug gate
    std::string GetGateString(Player* player) const;

    // Toggle per-player debug mode
    void SetDebugMode(Player* player, bool enable);

    // Clear interaction history and DB record for a player.
    void ClearHistory(Player* player);

    // Force-advance the current episode to its next phase (calls mechanic->ForceAdvance).
    void ForceAdvanceEpisode(Player* player);

    // Returns the active template ID for a player (0 if none).
    uint32 GetActiveTemplateId(Player* player) const;

    // Returns a const pointer to the player's active InteractionInstance (nullptr if none).
    const InteractionInstance* GetActiveInst(Player* player) const;

    // Returns the player's current DQ state (DQ_STATE_IDLE if no state exists).
    DQPlayerState GetPlayerState(Player* player) const;

    // Called by DQ_CourierAI::JustDied — dispatches OnKill to the active mechanic.
    void OnCourierKilled(Player* player, Creature* courier);

    // Ileana episode accessors (used by DQ_SuccubusAI).
    uint8  GetIleanaEpisode(Player* player) const;
    void   SetSoulDebt(Player* player, uint32 expiryTimestamp);

    // Called by DQ_DestinationAI / DQ_CourierAI on delivery attempt.
    void OnDestinationInteracted(Player* player, Creature* destNpc);

    // Called by DQ_PropScript when a player activates a prop GO.
    void OnPropActivated(Player* player, GameObject* go);

    // Config values (loaded once, refreshed by LoadConfig)
    uint32 cfg_tier1MinMs       = 1800000;
    uint32 cfg_tier1MaxMs       = 2700000;
    uint32 cfg_tier2MinMs       = 5400000;
    uint32 cfg_tier2MaxMs       = 9000000;
    uint32 cfg_activityWindowMs = 30000;
    uint32 cfg_historySize      = 10;
    uint32 cfg_tier2Chance      = 20;
    float  cfg_courierSpeedBonus = 1.25f;
    uint32 cfg_courierTimeoutMs  = 45000;
    bool   cfg_enabled          = true;
    bool   cfg_verbose          = false;

private:
    DynamicQuestMgr() = default;

    PlayerDQState& GetOrCreate(ObjectGuid guid);
    const PlayerDQState* Get(ObjectGuid guid) const;

    void TickCooldown(Player* player, PlayerDQState& ps, uint32 diff);
    void TickInbound(Player* player, PlayerDQState& ps, uint32 diff);
    void TickOnQuest(Player* player, PlayerDQState& ps, uint32 diff);
    void TryTrigger(Player* player, PlayerDQState& ps);
    bool SpawnCourier(Player* player, PlayerDQState& ps, uint32 templateId, const CourierSelection& sel);
    // Dispatches to the appropriate DQSpawnSystem call based on beat's spawn_style.
    TempSummon* SpawnWithStyle(Player* player, const DQSpawnDesc& desc,
        const std::string& style);
    void TransitionState(Player* player, PlayerDQState& ps, DQPlayerState newState);
    uint32 RollCooldown(uint8 tier) const;
    void AddToHistory(PlayerDQState& ps, uint32 templateId);

    // Returns the active mechanic for the given template ID, or nullptr.
    IMechanicModule* GetMechanic(uint32 templateId) const;

    void RegisterMechanics();

    // Async login callbacks — run on world thread via session QueryProcessor.
    void ApplyLoginData(ObjectGuid playerGuid, QueryResult result);
    void ApplyArchetypeData(ObjectGuid playerGuid, QueryResult result);

    // Per-player state map. Keyed by ObjectGuid raw value.
    std::unordered_map<uint64, PlayerDQState> _states;

    // Mechanic module registry. Keyed by DQMechanicType.
    std::unordered_map<uint8, std::unique_ptr<IMechanicModule>> _mechanics;

    // Lock-free phase slot allocator — shared across all player threads.
    DQPhasePool _phasePool;

    // Strips the player's private phase bit and returns it to the pool.
    // Safe to call even if no phase was assigned (activePhaseBit == 0).
    void ReleasePhase(Player* player, PlayerDQState& ps);
};

#define sDQMgr DynamicQuestMgr::instance()

#endif // DQ_DYNAMIC_QUEST_MGR_H
