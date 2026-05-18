/*
 * DynamicQuests+ Module
 * Emotion Engine v9 — Implementation
 *
 * Emote IDs are HandleEmoteCommand / Emote enum values (WoW 3.3.5a client).
 * Durations are hardcoded in GetEmoteDuration() — not stored in the DB.
 */

#include "DQEmotionEngine.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Language.h"
#include "Log.h"
#include "QueryResult.h"
#include "SharedDefines.h"

// ---------------------------------------------------------------------------
// DQEmoteSequencer
// ---------------------------------------------------------------------------

void DQEmoteSequencer::Play(const std::vector<DQEmoteStep>& steps)
{
    _queue   = steps;
    _head    = 0;
    _timer   = 0;
    _pending = true;
}

void DQEmoteSequencer::Clear()
{
    _queue.clear();
    _head    = 0;
    _timer   = 0;
    _pending = true;
}

void DQEmoteSequencer::Tick(Creature* c, uint32 diff)
{
    if (_head >= _queue.size())
        return;

    if (_pending)
    {
        const DQEmoteStep& step = _queue[_head];
        if (step.emote != 0)
            c->HandleEmoteCommand(static_cast<Emote>(step.emote));
        if (!step.sayLine.empty())
            c->Say(step.sayLine, LANG_UNIVERSAL);
        _timer   = step.durationMs;
        _pending = false;
        return;
    }

    if (_timer <= diff)
    {
        ++_head;
        _pending = true;
    }
    else
    {
        _timer -= diff;
    }
}

// ---------------------------------------------------------------------------
// DQEmotionEngine — singleton
// ---------------------------------------------------------------------------

DQEmotionEngine* DQEmotionEngine::instance()
{
    static DQEmotionEngine inst;
    return &inst;
}

const DQEmotionDef* DQEmotionEngine::GetEmotion(const std::string& name) const
{
    auto it = _emotions.find(name);
    return it != _emotions.end() ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// LoadFromDB — populates _emotions from dq_emotion table
// ---------------------------------------------------------------------------

void DQEmotionEngine::LoadFromDB()
{
    _emotions.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT name, opener_emote, talk_emote, emotion_emote, close_emote "
        "FROM dq_emotion");

    if (!result)
    {
        LOG_WARN("module.dynamicquests", "DQEmotionEngine: dq_emotion table is empty or missing.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* f = result->Fetch();
        DQEmotionDef def;
        def.name         = f[0].Get<std::string>();
        def.openerEmote  = f[1].Get<uint32>();
        def.talkEmote    = f[2].Get<uint32>();
        def.emotionEmote = f[3].Get<uint32>();
        def.closeEmote   = f[4].Get<uint32>();
        _emotions[def.name] = def;
        ++count;
    } while (result->NextRow());

    LOG_INFO("module.dynamicquests", "DQEmotionEngine: Loaded {} emotion definitions.", count);
}

// ---------------------------------------------------------------------------
// GetEmoteDuration — WoW 3.3.5a client animation lengths in ms
// ---------------------------------------------------------------------------

/*static*/ uint32 DQEmotionEngine::GetEmoteDuration(uint32 emoteId)
{
    switch (emoteId)
    {
        case 1:   return 2000;  // talk
        case 2:   return 2500;  // bow
        case 3:   return 2000;  // wave
        case 4:   return 3000;  // cheer
        case 11:  return 3000;  // laugh
        case 14:  return 2000;  // rude
        case 15:  return 2500;  // roar
        case 16:  return 2000;  // kneel
        case 18:  return 4000;  // cry
        case 20:  return 3000;  // beg
        case 21:  return 3000;  // applaud
        case 23:  return 3500;  // flex
        case 24:  return 2000;  // shy
        case 25:  return 2000;  // point
        case 66:  return 2500;  // salute
        case 76:  return 3500;  // plead
        case 273: return 1500;  // yes
        case 274: return 1500;  // no / shrug
        case 389: return 3500;  // grovel
        case 396: return 2000;  // talkex
        case 430: return 3000;  // cower
        default:  return 2000;
    }
}
