/*
 * DynamicQuests+ Module
 * System 4: Emotion/Emote Engine
 *
 * Brings archetype courier NPCs to life through intelligent, timed emote
 * sequences tied to emotional states. Authors write one emotion string in SQL;
 * the engine handles all timing, looping, context switching, and resolution.
 *
 * Two registries (loaded once at startup):
 *   _emotions    — 20 primary states (sad, urgent, reverent, ...) with sequences
 *                  for each of 5 context moments + pacing behaviour + decline reaction
 *   _resolutions — 10 resolution states (relieved, triumphant, ...) with onComplete
 *                  sequence played when a beat finishes
 *
 * DQEmoteSequencer is a lightweight queue owned by value inside DQ_CourierCreatureAI.
 * It fires one-shot emotes in order with per-step delays, driven by UpdateAI diff ticks.
 */

#ifndef DQ_EMOTION_ENGINE_H
#define DQ_EMOTION_ENGINE_H

#include "Define.h"
#include <string>
#include <unordered_map>
#include <vector>

class Creature;

// ---------------------------------------------------------------------------
// A single emote action within a sequence
// ---------------------------------------------------------------------------

struct DQEmoteStep
{
    uint32      emote      = 0;   // HandleEmoteCommand enum value; 0 = silent pause
    uint32      durationMs = 0;   // wait this many ms after firing before the next step
    std::string sayLine;          // optional NPC Say() fired together with the emote (empty = silent)
};

// ---------------------------------------------------------------------------
// DQEmoteSequencer — driven by value in DQ_CourierCreatureAI
//
// Usage:
//   Play(steps)       — replace current sequence; starts immediately on next Tick()
//   Clear()           — stop and empty the queue
//   Tick(c, diff)     — call every UpdateAI; fires emotes and counts down timers
//   IsPlaying()       — true while steps remain
// ---------------------------------------------------------------------------

class DQEmoteSequencer
{
public:
    void Play(const std::vector<DQEmoteStep>& steps);
    void Clear();
    void Tick(Creature* c, uint32 diff);
    bool IsPlaying() const { return !_queue.empty() && _head < _queue.size(); }

private:
    std::vector<DQEmoteStep> _queue;
    size_t  _head    = 0;
    uint32  _timer   = 0;
    bool    _pending = true; // true = fire current step on next Tick
};

// ---------------------------------------------------------------------------
// DQEmotionDef — full descriptor for one emotion or resolution state
// ---------------------------------------------------------------------------

struct DQEmotionDef
{
    std::string name;

    // Sequences for each context moment (primary emotions)
    std::vector<DQEmoteStep> onArrive;          // plays once when NPC stops at player
    std::vector<DQEmoteStep> onIdle;            // loops in DCP_ARRIVED while waiting
    std::vector<DQEmoteStep> onGossipOpen;      // plays 800ms after gossip menu opens
    std::vector<DQEmoteStep> onAccept;          // plays when player accepts

    // Decline reactions — which one fires depends on declineCategory
    std::vector<DQEmoteStep> onDeclineHurt;
    std::vector<DQEmoteStep> onDeclineFrustrated;
    std::vector<DQEmoteStep> onDeclineDignified;

    // Resolution sequence (resolution states only — plays on beat complete)
    std::vector<DQEmoteStep> onComplete;

    // Pacing behaviour (anxious/urgent/angry/etc. NPCs walk while waiting)
    bool  paces      = false;
    float paceRadius = 5.0f;

    // Default resolution if emotion_end is blank in the beat data
    std::string defaultResolution;

    // Which decline sequence category this emotion maps to
    // "hurt" | "frustrated" | "dignified"
    std::string declineCategory;
};

// ---------------------------------------------------------------------------
// DQEmotionEngine — singleton registry, initialised once at startup
// ---------------------------------------------------------------------------

class DQEmotionEngine
{
public:
    static DQEmotionEngine* instance();

    // Called from DynamicQuestMgr::Initialize() — registers all emotion defs.
    void Initialize();

    // Look up a primary emotion by name (e.g. "sad", "urgent").
    // Returns nullptr if name is unknown.
    const DQEmotionDef* GetEmotion(const std::string& name) const;

    // Look up a resolution state by name (e.g. "relieved", "triumphant").
    // Used by MechanicArchetype::CompleteBeat to fire the completion emote.
    const DQEmotionDef* GetResolution(const std::string& name) const;

    // Returns the auto-resolution name for an emotion when emotion_end is blank.
    // e.g. "sad" → "relieved".  Unknown emotion returns empty string.
    std::string GetDefaultResolution(const std::string& emotion) const;

private:
    DQEmotionEngine() = default;
    std::unordered_map<std::string, DQEmotionDef> _emotions;
    std::unordered_map<std::string, DQEmotionDef> _resolutions;
};

#define sDQEmotions DQEmotionEngine::instance()

#endif // DQ_EMOTION_ENGINE_H
