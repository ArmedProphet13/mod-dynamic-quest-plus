/*
 * DynamicQuests+ Module
 * Emotion Engine v9 — unified 4-slot model
 *
 * Author writes emotion = 'happy' on a beat; the engine resolves four emote IDs
 * (opener, talk, emotion_emote, close) loaded from the dq_emotion table.
 * DQ_CourierAI drives all timing automatically — no author input beyond the name.
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
// DQEmotionDef — 4-slot emotion descriptor loaded from dq_emotion
// ---------------------------------------------------------------------------

struct DQEmotionDef
{
    std::string name;
    uint32 openerEmote  = 0;   // fires when NPC stops at player (after MoveFollow ends)
    uint32 talkEmote    = 0;   // step 1 of the talk/emotion loop (fires when gossip opens)
    uint32 emotionEmote = 0;   // step 2 of the talk/emotion loop
    uint32 closeEmote   = 0;   // fires when player accepts or declines
};

// ---------------------------------------------------------------------------
// DQEmotionEngine — singleton registry, loaded from DB at startup
// ---------------------------------------------------------------------------

class DQEmotionEngine
{
public:
    static DQEmotionEngine* instance();

    // Called from DynamicQuestMgr::Initialize() — loads rows from dq_emotion.
    void LoadFromDB();

    // Look up an emotion by name (e.g. "happy", "angry").
    // Returns nullptr if the name is unknown.
    const DQEmotionDef* GetEmotion(const std::string& name) const;

    // Returns the WoW 3.3.5a client duration in ms for a given emote ID.
    // Used to arm timers without storing durations in the DB.
    static uint32 GetEmoteDuration(uint32 emoteId);

private:
    DQEmotionEngine() = default;
    std::unordered_map<std::string, DQEmotionDef> _emotions;
};

#define sDQEmotions DQEmotionEngine::instance()

#endif // DQ_EMOTION_ENGINE_H
