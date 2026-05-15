/*
 * DQ_CourierAI — generic archetype courier NPC (entry 900004 "Traveling Stranger")
 *
 * Handles all spawn styles through a single AI. The spawn style determines where
 * the courier appears; this AI drives the arrival sequence from there.
 *
 * Phases:
 *   DCP_APPROACHING — MoveFollow toward summoner player; stops at 3y
 *   DCP_ARRIVED     — Stopped, facing player. Player clicks to open gossip. 120s timeout.
 *   DCP_ACCEPTED    — Player accepted; mechanic running (AI stays alive for cleanup).
 *
 * Invisible spawn styles (from_portal, from_shadow): SetVisible(false) is applied
 * by SpawnWithStyle before the summon fully enters the world. InitializeAI detects
 * this and starts _revealTimer; the AI reveals itself and begins MoveFollow after
 * the delay.
 *
 * Gossip options:
 *   "I'll see to it."  / "I see."  — accept (label varies by beat mechanic)
 *   "Not right now."               — decline
 */

#include "ArchetypeMgr.h"
#include "CreatureScript.h"
#include "DynamicQuestMgr.h"
#include "GossipDef.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedAI/ScriptedCreature.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "TemporarySummon.h"
#include "ObjectAccessor.h"

static constexpr uint32 DQ_COURIER_SENDER   = 1203;
static constexpr uint32 ACTION_ACCEPT       = 0;
static constexpr uint32 ACTION_DECLINE      = 1;
static constexpr uint32 GOSSIP_TEXT_COURIER = 9000003; // blank gossip body — NPC already spoke
static constexpr float  ARRIVE_DIST         = 3.0f;
static constexpr uint32 WAIT_TIMEOUT_MS     = 120000;  // 2 min inactivity abort
static constexpr uint32 REVEAL_DELAY_MS     = 900;     // invisible spawn → visible after this

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
    {}

    void InitializeAI() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

        Player* p = SummonerPlayer();
        if (!p)
        {
            me->DespawnOrUnsummon(Milliseconds(0));
            return;
        }

        // Scale level so the courier doesn't appear as a level-1 peasant in a lvl80 zone
        me->SetLevel(p->GetLevel());

        if (!me->IsVisible())
        {
            // Invisible spawn (from_portal / from_shadow): arm reveal timer; do not MoveFollow yet
            _revealTimer = REVEAL_DELAY_MS;
        }
        else
        {
            me->GetMotionMaster()->MoveFollow(p, 1.5f, 0.f);
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

        // Reveal sequence for invisible spawn styles
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

        switch (_phase)
        {
            case DCP_APPROACHING: TickApproaching(player, diff); break;
            case DCP_ARRIVED:     TickArrived(diff);             break;
            default:                                              break;
        }
    }

    void HandleGossipHello(Player* player)
    {
        if (_phase != DCP_ARRIVED)
            return;

        // Choose accept label based on beat mechanic (witness = acknowledgement, other = task)
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
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, acceptText,        DQ_COURIER_SENDER, ACTION_ACCEPT);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Not right now.",  DQ_COURIER_SENDER, ACTION_DECLINE);
        SendGossipMenuFor(player, GOSSIP_TEXT_COURIER, me);
    }

    bool HandleGossipSelect(Player* player, uint32 action)
    {
        CloseGossipMenuFor(player);

        if (action == ACTION_ACCEPT)
        {
            _phase = DCP_ACCEPTED;
            sDQMgr->OnInteractionAccepted(player, sDQMgr->GetActiveTemplateId(player), 0);
            return true;
        }

        if (action == ACTION_DECLINE)
        {
            me->DespawnOrUnsummon(Milliseconds(2000));
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
    DCPhase _phase;
    uint32  _waitTimer;
    uint32  _revealTimer;

    void TickApproaching(Player* player, uint32 /*diff*/)
    {
        if (me->GetDistance(player) <= ARRIVE_DIST)
        {
            me->GetMotionMaster()->Clear();
            me->SetFacingToObject(player);

            _phase     = DCP_ARRIVED;
            _waitTimer = WAIT_TIMEOUT_MS;

            // OnCourierArrived → MechanicArchetype::OnStart: plays arrive emote + says greeting.
            // Player then clicks the NPC (? marker) to open gossip.
            sDQMgr->OnCourierArrived(player, me);
        }
    }

    void TickArrived(uint32 diff)
    {
        if (_waitTimer <= diff)
            Abort();
        else
            _waitTimer -= diff;
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
