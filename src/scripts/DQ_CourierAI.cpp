/*
 * DQ_CourierAI — generic archetype courier NPC (entry 900004 "Traveling Stranger")
 *
 * Phases:
 *   DCP_APPROACHING — MoveFollow toward summoner player; stops at 3y
 *   DCP_ARRIVED     — Stopped, facing player. Plays emotion sequences. 120s timeout.
 *   DCP_ACCEPTED    — Player accepted; mechanic running (AI stays alive for cleanup).
 *
 * Emotion system (System 4):
 *   At DCP_ARRIVED the AI reads beat->emotion from ArchetypeMgr, resolves a
 *   DQEmotionDef, and drives a DQEmoteSequencer through five context moments:
 *     on_arrive   — plays once when NPC stops
 *     on_idle     — loops until gossip or accept
 *     on_gossip   — plays 800ms after player opens gossip menu
 *     on_accept   — plays when player accepts; suppresses idle
 *     on_decline  — plays when player declines (category: hurt/frustrated/dignified)
 *   Pacing emotions (anxious, urgent, angry, …) walk to random nearby points.
 *
 * Invisible spawn styles (from_portal, from_shadow): SetVisible(false) is applied
 * by SpawnWithStyle before the summon enters the world. InitializeAI detects this
 * and arms _revealTimer; the AI reveals itself and begins MoveFollow after 900ms.
 */

#include "ArchetypeMgr.h"
#include "CreatureScript.h"
#include "DQEmotionEngine.h"
#include "DynamicQuestMgr.h"
#include "GossipDef.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedAI/ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include <cmath>

static constexpr uint32 DQ_COURIER_SENDER   = 1203;
static constexpr uint32 ACTION_ACCEPT       = 0;
static constexpr uint32 ACTION_DECLINE      = 1;
static constexpr uint32 GOSSIP_TEXT_COURIER = 9000003; // blank body — NPC already spoke via Say()
static constexpr float  ARRIVE_DIST         = 3.0f;
static constexpr uint32 WAIT_TIMEOUT_MS     = 120000;  // 2 min inactivity abort
static constexpr uint32 REVEAL_DELAY_MS     = 900;     // invisible spawn → visible after this ms
static constexpr uint32 GOSSIP_EMOTE_DELAY  = 800;     // ms after gossip opens before emote fires
static constexpr uint32 DECLINE_DESPAWN_MS  = 4000;    // ms before courier despawns after decline

enum DCPhase : uint8
{
    DCP_APPROACHING = 0,
    DCP_ARRIVED     = 1,
    DCP_ACCEPTED    = 2,
};

// ── AI ─────────────────────────────────────────────────────────────────────────

class DQ_CourierCreatureAI : public ScriptedAI
{
public:
    explicit DQ_CourierCreatureAI(Creature* c)
        : ScriptedAI(c)
        , _phase(DCP_APPROACHING)
        , _waitTimer(WAIT_TIMEOUT_MS)
        , _revealTimer(0)
        , _idleLooping(false)
        , _gossipDelay(0)
        , _paceTimer(0)
        , _emotionDef(nullptr)
    {}

    void InitializeAI() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

        Player* player = SummonerPlayer();
        if (!player)
        {
            me->DespawnOrUnsummon(Milliseconds(0));
            return;
        }

        me->SetLevel(player->GetLevel());

        // Resolve emotion from the active beat
        if (IsArchetypeId(sDQMgr->GetActiveTemplateId(player)))
        {
            if (const InteractionInstance* inst = sDQMgr->GetActiveInst(player))
            {
                const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(
                    DecodeArchetypeId(inst->templateId), inst->currentPhase);
                if (beat && !beat->emotion.empty())
                    _emotionDef = sDQEmotions->GetEmotion(beat->emotion);
            }
        }

        if (!me->IsVisible())
        {
            // Invisible spawn (from_portal / from_shadow): arm reveal timer
            _revealTimer = REVEAL_DELAY_MS;
        }
        else
        {
            me->GetMotionMaster()->MoveFollow(player, 1.5f, 0.f);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        Player* player = SummonerPlayer();
        if (!player || !player->IsAlive())
        {
            Abort();
            return;
        }

        // Invisible reveal sequence
        if (_revealTimer)
        {
            if (_revealTimer <= diff)
            {
                _revealTimer = 0;
                me->SetVisible(true);
                me->GetMotionMaster()->MoveFollow(player, 1.5f, 0.f);
            }
            else
            {
                _revealTimer -= diff;
                return;
            }
        }

        // Emote sequencer ticks regardless of phase (handles accept/decline emotes too)
        _sequencer.Tick(me, diff);

        switch (_phase)
        {
            case DCP_APPROACHING: TickApproaching(player, diff); break;
            case DCP_ARRIVED:     TickArrived(player, diff);     break;
            default:                                              break;
        }
    }

    void HandleGossipHello(Player* player)
    {
        if (_phase != DCP_ARRIVED)
            return;

        // Stop idle loop; arm emote delay
        _idleLooping = false;
        _gossipDelay = GOSSIP_EMOTE_DELAY;

        const char* acceptText = "I'll see to it.";
        uint32 templateId = sDQMgr->GetActiveTemplateId(player);
        if (IsArchetypeId(templateId))
        {
            if (const InteractionInstance* inst = sDQMgr->GetActiveInst(player))
            {
                const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(
                    DecodeArchetypeId(templateId), inst->currentPhase);
                if (beat && beat->mechanic == DQ_BEAT_WITNESS)
                    acceptText = "I see.";
            }
        }

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, acceptText,       DQ_COURIER_SENDER, ACTION_ACCEPT);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Not right now.", DQ_COURIER_SENDER, ACTION_DECLINE);
        SendGossipMenuFor(player, GOSSIP_TEXT_COURIER, me);
    }

    bool HandleGossipSelect(Player* player, uint32 action)
    {
        CloseGossipMenuFor(player);

        if (action == ACTION_ACCEPT)
        {
            _phase       = DCP_ACCEPTED;
            _idleLooping = false;
            _paceTimer   = 0;
            me->GetMotionMaster()->Clear();
            me->SetFacingToObject(player);

            // on_accept emote
            if (_emotionDef && !_emotionDef->onAccept.empty())
            {
                _sequencer.Play(_emotionDef->onAccept);
            }

            // text_on_accept say
            uint32 templateId = sDQMgr->GetActiveTemplateId(player);
            if (IsArchetypeId(templateId))
            {
                if (const InteractionInstance* inst = sDQMgr->GetActiveInst(player))
                {
                    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(
                        DecodeArchetypeId(templateId), inst->currentPhase);
                    if (beat && !beat->textOnAccept.empty())
                        me->Say(beat->textOnAccept, LANG_UNIVERSAL);
                }
            }

            sDQMgr->OnInteractionAccepted(player, templateId, 0);
            return true;
        }

        if (action == ACTION_DECLINE)
        {
            _idleLooping = false;
            _paceTimer   = 0;

            // Play category-matched decline emote sequence
            if (_emotionDef)
            {
                const std::vector<DQEmoteStep>* seq = nullptr;
                if (_emotionDef->declineCategory == "hurt")
                    seq = &_emotionDef->onDeclineHurt;
                else if (_emotionDef->declineCategory == "frustrated")
                    seq = &_emotionDef->onDeclineFrustrated;
                else
                    seq = &_emotionDef->onDeclineDignified;

                if (seq && !seq->empty())
                    _sequencer.Play(*seq);
            }

            me->DespawnOrUnsummon(Milliseconds(DECLINE_DESPAWN_MS));
            sDQMgr->OnInteractionDeclined(player);
            return true;
        }

        return false;
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Player* p = SummonerPlayer())
        {
            CloseGossipMenuFor(p);
            sDQMgr->OnInteractionFailed(p);
        }
    }

private:
    DCPhase             _phase;
    uint32              _waitTimer;
    uint32              _revealTimer;
    DQEmoteSequencer    _sequencer;
    bool                _idleLooping;
    uint32              _gossipDelay;
    uint32              _paceTimer;
    const DQEmotionDef* _emotionDef;

    void TickApproaching(Player* player, uint32 /*diff*/)
    {
        if (me->GetDistance(player) > ARRIVE_DIST)
            return;

        me->GetMotionMaster()->Clear();
        me->SetFacingToObject(player);

        _phase     = DCP_ARRIVED;
        _waitTimer = WAIT_TIMEOUT_MS;

        // OnCourierArrived → MechanicArchetype::OnStart: caches inst state, greeting.
        sDQMgr->OnCourierArrived(player, me);

        // Start arrive emote sequence; arm idle loop for when it finishes
        if (_emotionDef && !_emotionDef->onArrive.empty())
        {
            _sequencer.Play(_emotionDef->onArrive);
            _idleLooping = true;
        }

        // Arm pacing if this emotion paces
        if (_emotionDef && _emotionDef->paces)
            _paceTimer = 2000u + uint32(rand() % 2001); // first pace 2-4s after arrival
    }

    void TickArrived(Player* player, uint32 diff)
    {
        // Timeout
        if (_waitTimer <= diff)
        {
            Abort();
            return;
        }
        _waitTimer -= diff;

        // Restart idle loop when arrive (or previous idle) finishes
        if (_idleLooping && !_sequencer.IsPlaying() && _emotionDef && !_emotionDef->onIdle.empty())
        {
            _sequencer.Play(_emotionDef->onIdle);
        }

        // Gossip-open emote countdown (armed by HandleGossipHello)
        if (_gossipDelay > 0)
        {
            if (_gossipDelay <= diff)
            {
                _gossipDelay = 0;
                if (_emotionDef && !_emotionDef->onGossipOpen.empty())
                    _sequencer.Play(_emotionDef->onGossipOpen);
            }
            else
            {
                _gossipDelay -= diff;
            }
        }

        // Pacing — pick a random nearby point and walk to it
        if (_paceTimer > 0 && _emotionDef && _emotionDef->paces)
        {
            if (_paceTimer <= diff)
            {
                _paceTimer = 4000u + uint32(rand() % 4001); // next pace 4-8s later
                float angle = float(rand()) / float(RAND_MAX) * 2.0f * float(M_PI);
                float dist  = float(rand()) / float(RAND_MAX) * _emotionDef->paceRadius;
                float px    = me->GetPositionX() + dist * std::cos(angle);
                float py    = me->GetPositionY() + dist * std::sin(angle);
                me->GetMotionMaster()->MovePoint(1, px, py, me->GetPositionZ());
            }
            else
            {
                _paceTimer -= diff;
            }
        }

        // Face back toward player after each pace completes (approximate: face if standing still)
        if (_emotionDef && _emotionDef->paces && !me->isMoving())
            me->SetFacingToObject(player);
    }

    void Abort()
    {
        if (Player* p = SummonerPlayer())
        {
            CloseGossipMenuFor(p);
            sDQMgr->OnInteractionFailed(p);
        }
        me->DespawnOrUnsummon(Milliseconds(0));
    }

    Player* SummonerPlayer()
    {
        if (!me->IsSummon()) return nullptr;
        WorldObject* s = me->ToTempSummon()->GetSummoner();
        return s ? s->ToPlayer() : nullptr;
    }
};

// ── CreatureScript ─────────────────────────────────────────────────────────────

class DQ_CourierScript : public CreatureScript
{
public:
    DQ_CourierScript() : CreatureScript("DQ_CourierAI") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (DQ_CourierCreatureAI* ai = CAST_AI(DQ_CourierCreatureAI, creature->AI()))
            ai->HandleGossipHello(player);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
        uint32 /*sender*/, uint32 action) override
    {
        if (DQ_CourierCreatureAI* ai = CAST_AI(DQ_CourierCreatureAI, creature->AI()))
            ai->HandleGossipSelect(player, action);
        return true;
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new DQ_CourierCreatureAI(creature);
    }
};

void AddSC_DQ_CourierAI()
{
    new DQ_CourierScript();
}
