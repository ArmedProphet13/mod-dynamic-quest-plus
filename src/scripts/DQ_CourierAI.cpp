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
#include "Bag.h"
#include "CreatureScript.h"
#include "DQClientSession.h"
#include "DQDialogueMgr.h"
#include "DQEmotionEngine.h"
#include "DQLog.h"
#include "DynamicQuestMgr.h"
#include "GossipDef.h"
#include "Item.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedAI/ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include <cmath>

// Searches main bag and equipped bags for the first item matching itemClass/itemSubclass.
// itemSubclass == 0 matches any subclass. Returns nullptr if nothing found.
static Item* FindItemByClassSubclass(Player* player, uint8 itemClass, uint8 itemSubclass)
{
    // Main backpack
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(NULL_BAG, i);
        if (!item) continue;
        ItemTemplate const* tmpl = item->GetTemplate();
        if (tmpl->Class == itemClass && (itemSubclass == 0 || tmpl->SubClass == itemSubclass))
            return item;
    }
    // Equipped bag slots
    for (uint8 b = INVENTORY_SLOT_BAG_START; b < INVENTORY_SLOT_BAG_END; ++b)
    {
        Bag* bag = player->GetBagByPos(b);
        if (!bag) continue;
        for (uint32 j = 0; j < bag->GetBagSize(); ++j)
        {
            Item* item = bag->GetItemByPos(j);
            if (!item) continue;
            ItemTemplate const* tmpl = item->GetTemplate();
            if (tmpl->Class == itemClass && (itemSubclass == 0 || tmpl->SubClass == itemSubclass))
                return item;
        }
    }
    return nullptr;
}

// DQ_COURIER_SENDER, GOSSIP_TEXT_COURIER, DQ_GOSSIP_DECLINE come from DQDialogueMgr.h
static constexpr float  ARRIVE_DIST         = 3.0f;
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
        , _lastMenuId(0)
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
        me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP); // ensure client sends CMSG_GOSSIP_HELLO, not CMSG_QUESTGIVER_HELLO

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

    // Fires before OnGossipSelect — captures the menuId the client echoed back.
    void sGossipSelect(Player* /*player*/, uint32 menuId, uint32 /*gossipListId*/) override
    {
        _lastMenuId = menuId;
    }

    void HandleGossipHello(Player* player)
    {
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "GossipHello: phase={}", static_cast<int>(_phase));

        if (_phase != DCP_ARRIVED)
        {
            DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "GossipHello: SKIP — not DCP_ARRIVED");
            return;
        }

        _idleLooping = false;
        _gossipDelay = GOSSIP_EMOTE_DELAY;

        uint32 templateId = sDQMgr->GetActiveTemplateId(player);
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "GossipHello: templateId={} isArchetype={}", templateId, IsArchetypeId(templateId));

        if (!IsArchetypeId(templateId))
        {
            DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "GossipHello: SKIP — templateId not an archetype");
            return;
        }

        const InteractionInstance* inst = sDQMgr->GetActiveInst(player);
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "GossipHello: inst={} phase={}", inst ? "valid" : "null",
            inst ? static_cast<int>(inst->currentPhase) : -1);

        if (!inst)
            return;

        uint32 archetypeId = DecodeArchetypeId(templateId);
        const ArchetypeDef*  def  = sArchetypeMgr->Get(archetypeId);
        const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst->currentPhase);
        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "GossipHello: archetypeId={} def={} beat={}",
            archetypeId, def ? "valid" : "null", beat ? "valid" : "null");

        if (!def || !beat)
        {
            DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "GossipHello: SKIP — def or beat not found");
            return;
        }

        DQ_LOG_DEBUG(DQ_LOG_CAT_SESSION, player, "GossipHello: opening session");
        DQClientSession::Open(player, me, *def, *beat,
            sDQMgr->PendingSession(player), sDQMgr->cfg_courierTimeoutMs);
    }

    bool HandleGossipSelect(Player* player, Creature* creature, uint32 action)
    {
        // Validate the response against the session we opened.
        DQPendingSession& sess = sDQMgr->PendingSession(player);
        if (!DQClientSession::Validate(player, creature, _lastMenuId, sess))
            return false;
        DQClientSession::Close(sess);

        CloseGossipMenuFor(player);

        if (action == DQ_GOSSIP_DECLINE)
        {
            _idleLooping = false;
            _paceTimer   = 0;

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

        // Accept path: action == 0 for all non-branching mechanics,
        // or 0-3 as choice index for BRANCHING pattern beats.
        _phase       = DCP_ACCEPTED;
        _idleLooping = false;
        _paceTimer   = 0;
        me->GetMotionMaster()->Clear();
        me->SetFacingToObject(player);

        if (_emotionDef && !_emotionDef->onAccept.empty())
            _sequencer.Play(_emotionDef->onAccept);

        uint32 templateId = sDQMgr->GetActiveTemplateId(player);
        if (IsArchetypeId(templateId))
        {
            if (const InteractionInstance* inst = sDQMgr->GetActiveInst(player))
            {
                const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(
                    DecodeArchetypeId(templateId), inst->currentPhase);
                if (beat)
                {
                    if (!beat->textOnAccept.empty())
                        me->Say(beat->textOnAccept, LANG_UNIVERSAL);

                    // Consume one matching item when the COURIER mechanic requires it.
                    if (beat->itemConsume && beat->itemPrereqClass != 0)
                    {
                        Item* donated = FindItemByClassSubclass(
                            player, beat->itemPrereqClass, beat->itemPrereqSubclass);
                        if (donated)
                            player->DestroyItem(donated->GetBagSlot(), donated->GetSlot(), true);
                    }
                }
            }
        }

        // Pass action through so BRANCHING mechanics receive the correct choice index.
        sDQMgr->OnInteractionAccepted(player, templateId, action);
        return true;
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
    uint32              _lastMenuId;  // menuId echoed by client in sGossipSelect
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

        _phase = DCP_ARRIVED;

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
            ai->HandleGossipSelect(player, creature, action);
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
