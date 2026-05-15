/*
 * DynamicQuests+ Module
 * System 4: Emotion/Emote Engine — Implementation
 *
 * Emote ID reference (SharedDefines.h Emote enum):
 *   EMOTE_ONESHOT_TALK        = 1   EMOTE_ONESHOT_BOW      = 2
 *   EMOTE_ONESHOT_WAVE        = 3   EMOTE_ONESHOT_CHEER    = 4
 *   EMOTE_ONESHOT_EXCLAMATION = 5   EMOTE_ONESHOT_QUESTION = 6
 *   EMOTE_ONESHOT_LAUGH       = 11  EMOTE_ONESHOT_RUDE     = 14
 *   EMOTE_ONESHOT_KNEEL       = 16  EMOTE_ONESHOT_CRY      = 18
 *   EMOTE_ONESHOT_BEG         = 20  EMOTE_ONESHOT_APPLAUD  = 21
 *   EMOTE_ONESHOT_SHOUT       = 22  EMOTE_ONESHOT_FLEX     = 23
 *   EMOTE_ONESHOT_SHY         = 24  EMOTE_ONESHOT_POINT    = 25
 *   EMOTE_ONESHOT_SALUTE      = 66  EMOTE_ONESHOT_DANCE    = 94
 *   EMOTE_ONESHOT_YES         = 273 EMOTE_ONESHOT_NO       = 274
 *   EMOTE_ONESHOT_COWER       = 430
 */

#include "DQEmotionEngine.h"
#include "Creature.h"
#include "Language.h"
#include "SharedDefines.h"

// ---------------------------------------------------------------------------
// Shorthand helpers — local to this file only
// ---------------------------------------------------------------------------

namespace
{
    // Build a step that fires an emote and waits durationMs before the next step.
    inline DQEmoteStep E(uint32 emote, uint32 durationMs, const char* say = "")
    {
        return DQEmoteStep{emote, durationMs, say};
    }

    // Build a pure silent pause (no emote fired).
    inline DQEmoteStep P(uint32 durationMs)
    {
        return DQEmoteStep{0, durationMs, ""};
    }

    // Shorthand emote constants mirroring SharedDefines.h values.
    // These exist so Initialize() reads like a table, not a wall of enum names.
    constexpr uint32 eTALK  = EMOTE_ONESHOT_TALK;        // 1   talking gesture
    constexpr uint32 eBOW   = EMOTE_ONESHOT_BOW;         // 2   respectful bow
    constexpr uint32 eWAVE  = EMOTE_ONESHOT_WAVE;        // 3   friendly wave
    constexpr uint32 eCHEER = EMOTE_ONESHOT_CHEER;       // 4   arms-up cheer
    constexpr uint32 eEXCL  = EMOTE_ONESHOT_EXCLAMATION; // 5   surprised exclamation
    constexpr uint32 eQUEST = EMOTE_ONESHOT_QUESTION;    // 6   shrug / question
    constexpr uint32 eLAUGH = EMOTE_ONESHOT_LAUGH;       // 11  laugh
    constexpr uint32 eRUDE  = EMOTE_ONESHOT_RUDE;        // 14  dismissive rude gesture
    constexpr uint32 eKNEEL = EMOTE_ONESHOT_KNEEL;       // 16  kneel / pray
    constexpr uint32 eCRY   = EMOTE_ONESHOT_CRY;         // 18  cry / weep
    constexpr uint32 eBEG   = EMOTE_ONESHOT_BEG;         // 20  beg / plead
    constexpr uint32 eCLAP  = EMOTE_ONESHOT_APPLAUD;     // 21  clap / applaud
    constexpr uint32 eSHOUT = EMOTE_ONESHOT_SHOUT;       // 22  angry shout
    constexpr uint32 eFLEX  = EMOTE_ONESHOT_FLEX;        // 23  flex / victory pose
    constexpr uint32 eSHY   = EMOTE_ONESHOT_SHY;         // 24  shy look / subtle sigh
    constexpr uint32 ePOINT = EMOTE_ONESHOT_POINT;       // 25  point
    constexpr uint32 eSALUT = EMOTE_ONESHOT_SALUTE;      // 66  military salute
    constexpr uint32 eDANCE = EMOTE_ONESHOT_DANCE;       // 94  dance
    constexpr uint32 eYES   = EMOTE_ONESHOT_YES;         // 273 nod yes
    constexpr uint32 eNO    = EMOTE_ONESHOT_NO;          // 274 shake head no
    constexpr uint32 eCOWER = EMOTE_ONESHOT_COWER;       // 430 cower in fear
} // anonymous namespace

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

const DQEmotionDef* DQEmotionEngine::GetResolution(const std::string& name) const
{
    auto it = _resolutions.find(name);
    return it != _resolutions.end() ? &it->second : nullptr;
}

std::string DQEmotionEngine::GetDefaultResolution(const std::string& emotion) const
{
    auto it = _emotions.find(emotion);
    return it != _emotions.end() ? it->second.defaultResolution : "";
}

// ---------------------------------------------------------------------------
// Initialize — define all 20 primary emotions + 10 resolution states
// ---------------------------------------------------------------------------

void DQEmotionEngine::Initialize()
{
    // ── Primary emotions ────────────────────────────────────────────────────
    // Each entry: paces, paceRadius, declineCategory, defaultResolution,
    // and per-context sequences: onArrive, onIdle, onGossipOpen, onAccept,
    // plus three decline variants (only one fires based on declineCategory).

    // 1. sad — grief, loss, sorrow. Quiet. Doesn't pace.
    {
        DQEmotionDef& d = _emotions["sad"];
        d.name = "sad"; d.paces = false;
        d.declineCategory = "hurt"; d.defaultResolution = "relieved";
        d.onArrive      = {E(eCRY,3500), E(eTALK,2500)};
        d.onIdle        = {E(eCRY,3500), P(1500), E(eTALK,2500), P(2000), E(eSHY,2500), P(2500)};
        d.onGossipOpen  = {E(eBEG,3500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eTALK,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 2. mournful — deeper grief, ritual mourning. Still. Kneels.
    {
        DQEmotionDef& d = _emotions["mournful"];
        d.name = "mournful"; d.paces = false;
        d.declineCategory = "hurt"; d.defaultResolution = "peaceful";
        d.onArrive      = {E(eKNEEL,2500), E(eCRY,3500)};
        d.onIdle        = {E(eKNEEL,2500), P(2000), E(eCRY,3500), P(1500), E(eTALK,2500), P(3000)};
        d.onGossipOpen  = {E(eKNEEL,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eKNEEL,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eKNEEL,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 3. solemn — measured, grave, purposeful. Military bearing.
    {
        DQEmotionDef& d = _emotions["solemn"];
        d.name = "solemn"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "solemn";
        d.onArrive      = {E(eSALUT,3000), E(eTALK,2500)};
        d.onIdle        = {E(eTALK,2500), P(2000), E(eSALUT,3000), P(2000), E(eQUEST,2500), P(3000)};
        d.onGossipOpen  = {E(eBOW,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eSALUT,3000)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 4. anxious — nervous energy. Paces restlessly.
    {
        DQEmotionDef& d = _emotions["anxious"];
        d.name = "anxious"; d.paces = true; d.paceRadius = 5.0f;
        d.declineCategory = "frustrated"; d.defaultResolution = "relieved";
        d.onArrive      = {E(eTALK,2500), E(eQUEST,2500), E(eSHY,2500)};
        d.onIdle        = {E(eQUEST,2500), P(1000), E(eSHOUT,3000), P(1500), E(eTALK,2500), P(1500)};
        d.onGossipOpen  = {E(eEXCL,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eEXCL,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 5. urgent — something must happen NOW. Sprints in (pairs with run_up spawn style).
    {
        DQEmotionDef& d = _emotions["urgent"];
        d.name = "urgent"; d.paces = true; d.paceRadius = 4.0f;
        d.declineCategory = "frustrated"; d.defaultResolution = "relieved";
        d.onArrive      = {E(eEXCL,2500), E(ePOINT,2500), E(eTALK,2500)};
        d.onIdle        = {E(eSHOUT,3000), P(1000), E(eQUEST,2500), P(1000), E(ePOINT,2500), P(1000)};
        d.onGossipOpen  = {E(eEXCL,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eEXCL,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eNO,2000)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 6. frightened — fear, danger. Flinches and cowers. Paces nervously.
    {
        DQEmotionDef& d = _emotions["frightened"];
        d.name = "frightened"; d.paces = true; d.paceRadius = 3.0f;
        d.declineCategory = "hurt"; d.defaultResolution = "relieved";
        d.onArrive      = {E(eCOWER,2500), E(eTALK,2500)};
        d.onIdle        = {E(eCOWER,2500), P(1500), E(eTALK,2500), P(1500), E(eCOWER,2500), P(2000)};
        d.onGossipOpen  = {E(eCOWER,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eTALK,2500)};
        d.onDeclineHurt       = {E(eCOWER,2500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eCOWER,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 7. sick — nausea, illness, pain. Hunched. Doesn't pace.
    {
        DQEmotionDef& d = _emotions["sick"];
        d.name = "sick"; d.paces = false;
        d.declineCategory = "hurt"; d.defaultResolution = "relieved";
        d.onArrive      = {E(eSHY,2500), E(eCRY,3500)};
        d.onIdle        = {E(eSHY,2500), P(1500), E(eCRY,3500), P(2000), E(eTALK,2500), P(2500)};
        d.onGossipOpen  = {E(eTALK,2500), E(eSHY,2500)};
        d.onAccept      = {E(eYES,2000), E(eSHY,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 8. exhausted — drained, spent, barely standing.
    {
        DQEmotionDef& d = _emotions["exhausted"];
        d.name = "exhausted"; d.paces = false;
        d.declineCategory = "hurt"; d.defaultResolution = "peaceful";
        d.onArrive      = {E(eKNEEL,2500), E(eSHY,2500), E(eTALK,2500)};
        d.onIdle        = {E(eSHY,2500), P(2500), E(eTALK,2500), P(3000), E(eKNEEL,2500), P(2500)};
        d.onGossipOpen  = {E(eSHY,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eSHY,2500)};
        d.onDeclineHurt       = {E(eSHY,2500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 9. proud — dignity, accomplishment, self-assurance.
    {
        DQEmotionDef& d = _emotions["proud"];
        d.name = "proud"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "triumphant";
        d.onArrive      = {E(eCHEER,3500), E(eSALUT,3000)};
        d.onIdle        = {E(eCHEER,3500), P(2000), E(eSALUT,3000), P(2000), E(eTALK,2500), P(2000)};
        d.onGossipOpen  = {E(eSALUT,3000), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eCHEER,3500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eSALUT,3000), E(eBOW,2500)};
    }

    // 10. determined — focused resolve. Stands firm, not pacing.
    {
        DQEmotionDef& d = _emotions["determined"];
        d.name = "determined"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "triumphant";
        d.onArrive      = {E(eSALUT,3000), E(eTALK,2500)};
        d.onIdle        = {E(eTALK,2500), P(2000), E(ePOINT,2500), P(1500), E(eSALUT,3000), P(2000)};
        d.onGossipOpen  = {E(ePOINT,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eSALUT,3000)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 11. curious — inquisitive, interested, examining.
    {
        DQEmotionDef& d = _emotions["curious"];
        d.name = "curious"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "grateful";
        d.onArrive      = {E(eQUEST,2500), E(eTALK,2500), E(ePOINT,2500)};
        d.onIdle        = {E(eQUEST,2500), P(2000), E(ePOINT,2500), P(1500), E(eTALK,2500), P(2000)};
        d.onGossipOpen  = {E(eQUEST,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eTALK,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eQUEST,2500), E(eSHOUT,3000)};
        d.onDeclineDignified  = {E(eQUEST,2500), E(eWAVE,2500)};
    }

    // 12. happy — genuine warmth and joy.
    {
        DQEmotionDef& d = _emotions["happy"];
        d.name = "happy"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "joyful";
        d.onArrive      = {E(eWAVE,2500), E(eCHEER,3500)};
        d.onIdle        = {E(eCHEER,3500), P(2000), E(eLAUGH,3500), P(1500), E(eWAVE,2500), P(2000)};
        d.onGossipOpen  = {E(eWAVE,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eCHEER,3500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eWAVE,2500), E(eSHY,2500)};
    }

    // 13. playful — light, fun, teasing.
    {
        DQEmotionDef& d = _emotions["playful"];
        d.name = "playful"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "joyful";
        d.onArrive      = {E(eWAVE,2500), E(eLAUGH,3500)};
        d.onIdle        = {E(eLAUGH,3500), P(1000), E(eDANCE,3500), P(1000), E(eCHEER,3500), P(2000)};
        d.onGossipOpen  = {E(eLAUGH,3500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eLAUGH,3500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eLAUGH,3500), E(eWAVE,2500)};
    }

    // 14. hopeful — earnest, vulnerable optimism. Pleads a little.
    {
        DQEmotionDef& d = _emotions["hopeful"];
        d.name = "hopeful"; d.paces = false;
        d.declineCategory = "hurt"; d.defaultResolution = "moved";
        d.onArrive      = {E(eTALK,2500), E(eBEG,3500)};
        d.onIdle        = {E(eBEG,3500), P(2000), E(eTALK,2500), P(2000), E(eKNEEL,2500), P(2000)};
        d.onGossipOpen  = {E(eBEG,3500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eCLAP,3500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 15. angry — frustration that has boiled over. Paces aggressively.
    {
        DQEmotionDef& d = _emotions["angry"];
        d.name = "angry"; d.paces = true; d.paceRadius = 6.0f;
        d.declineCategory = "frustrated"; d.defaultResolution = "resigned";
        d.onArrive      = {E(eSHOUT,3000), E(ePOINT,2500), E(eTALK,2500)};
        d.onIdle        = {E(eSHOUT,3000), P(1000), E(ePOINT,2500), P(1000), E(eSHOUT,3000), P(1500)};
        d.onGossipOpen  = {E(eSHOUT,3000), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eTALK,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eRUDE,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 16. bitter — cold resentment, quiet hostility. Paces slowly.
    {
        DQEmotionDef& d = _emotions["bitter"];
        d.name = "bitter"; d.paces = true; d.paceRadius = 4.0f;
        d.declineCategory = "frustrated"; d.defaultResolution = "resigned";
        d.onArrive      = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onIdle        = {E(eQUEST,2500), P(2000), E(eSHOUT,3000), P(1500), E(eNO,2000), P(2000)};
        d.onGossipOpen  = {E(eQUEST,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eTALK,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eQUEST,2500), E(eSHOUT,3000)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 17. mysterious — enigmatic, deliberate, unhurried. Pairs with from_shadow.
    {
        DQEmotionDef& d = _emotions["mysterious"];
        d.name = "mysterious"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "peaceful";
        d.onArrive      = {E(eBOW,2500), E(eTALK,2500)};
        d.onIdle        = {E(eTALK,2500), P(3000), E(eBOW,2500), P(4000), E(ePOINT,2500), P(3000)};
        d.onGossipOpen  = {E(eBOW,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eBOW,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 18. confused — disoriented, searching for answers. Paces aimlessly.
    {
        DQEmotionDef& d = _emotions["confused"];
        d.name = "confused"; d.paces = true; d.paceRadius = 4.0f;
        d.declineCategory = "frustrated"; d.defaultResolution = "relieved";
        d.onArrive      = {E(eQUEST,2500), E(eTALK,2500), E(eQUEST,2500)};
        d.onIdle        = {E(eQUEST,2500), P(1500), E(eTALK,2500), P(1500), E(eQUEST,2500), P(2000)};
        d.onGossipOpen  = {E(eQUEST,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eQUEST,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // 19. reverent — sacred awe, profound respect. Kneels. Pairs with from_portal.
    {
        DQEmotionDef& d = _emotions["reverent"];
        d.name = "reverent"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "transcendent";
        d.onArrive      = {E(eKNEEL,2500), E(eBOW,2500), E(eTALK,2500)};
        d.onIdle        = {E(eKNEEL,2500), P(2000), E(eBOW,2500), P(2000), E(eTALK,2500), P(2000)};
        d.onGossipOpen  = {E(eBOW,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eKNEEL,2500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eSALUT,3000)};
    }

    // 20. grateful — thankful, warm, overflowing appreciation.
    {
        DQEmotionDef& d = _emotions["grateful"];
        d.name = "grateful"; d.paces = false;
        d.declineCategory = "dignified"; d.defaultResolution = "joyful";
        d.onArrive      = {E(eBOW,2500), E(eCLAP,3500), E(eTALK,2500)};
        d.onIdle        = {E(eCLAP,3500), P(2000), E(eBOW,2500), P(2000), E(eTALK,2500), P(2000)};
        d.onGossipOpen  = {E(eBOW,2500), E(eTALK,2500)};
        d.onAccept      = {E(eYES,2000), E(eCLAP,3500)};
        d.onDeclineHurt       = {E(eCRY,3500), E(eWAVE,2500)};
        d.onDeclineFrustrated = {E(eSHOUT,3000), E(eQUEST,2500)};
        d.onDeclineDignified  = {E(eBOW,2500), E(eWAVE,2500)};
    }

    // ── Resolution states ────────────────────────────────────────────────────
    // Each has only onComplete populated. First step is played synchronously
    // in MechanicArchetype::CompleteBeat just before the courier despawns.

    // relieved — tension released, breathing again
    _resolutions["relieved"].name = "relieved";
    _resolutions["relieved"].onComplete = {E(eSHY,2500), E(eWAVE,2500), E(eTALK,2500)};

    // peaceful — quiet calm, acceptance
    _resolutions["peaceful"].name = "peaceful";
    _resolutions["peaceful"].onComplete = {E(eBOW,2500), E(eKNEEL,2500)};

    // solemn — grave acknowledgement, duty done
    _resolutions["solemn"].name = "solemn";
    _resolutions["solemn"].onComplete = {E(eSALUT,3000), E(eBOW,2500)};

    // triumphant — hard-won victory, justified pride
    _resolutions["triumphant"].name = "triumphant";
    _resolutions["triumphant"].onComplete = {E(eFLEX,4000), E(eCHEER,3500)};

    // joyful — pure delight, warmth shared
    _resolutions["joyful"].name = "joyful";
    _resolutions["joyful"].onComplete = {E(eCHEER,3500), E(eLAUGH,3500), E(eWAVE,2500)};

    // moved — touched deeply, almost speechless
    _resolutions["moved"].name = "moved";
    _resolutions["moved"].onComplete = {E(eCLAP,3500), E(eBOW,2500), E(eSHY,2500)};

    // grateful — deep thanks, humble
    _resolutions["grateful"].name = "grateful";
    _resolutions["grateful"].onComplete = {E(eBOW,2500), E(eCLAP,3500), E(eTALK,2500)};

    // proud — earned standing, delivered as stated
    _resolutions["proud"].name = "proud";
    _resolutions["proud"].onComplete = {E(eCHEER,3500), E(eSALUT,3000)};

    // resigned — weary acceptance, nothing more to give
    _resolutions["resigned"].name = "resigned";
    _resolutions["resigned"].onComplete = {E(eQUEST,2500), E(eWAVE,2500)};

    // transcendent — beyond words, sacred completion
    _resolutions["transcendent"].name = "transcendent";
    _resolutions["transcendent"].onComplete = {E(eKNEEL,2500), E(eBOW,2500), E(eCLAP,3500)};
}
