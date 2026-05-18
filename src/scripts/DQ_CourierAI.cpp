/*
 * DQ_CourierAI — generic archetype courier NPC (entry 900004 "Traveling Stranger")
 *
 * Phases:
 *   DCP_APPROACHING — MoveFollow toward summoner player; stops at 3y
 *   DCP_ARRIVED     — Stopped, facing player. Drives talk/emotion loop. 120s timeout.
 *   DCP_ACCEPTED    — Player accepted; mechanic running (AI stays alive for cleanup).
 *
 * Emote system (v9 — unified 4-slot):
 *   opener     — fires when NPC stops at player (openerEmote)
 *   talk loop  — talkEmote → emotionEmote → talkEmote → ... while gossip is open
 *   close      — fires at the next step boundary after player responds
 *
 * Step-boundary interrupt rule: player response sets _closePending.
 * Close fires when the current step timer expires naturally — never mid-animation.
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

// Searches main bag and equipped bags for the first item matching itemClass/itemSubclass.
// itemSubclass == 0 matches any subclass. Returns nullptr if nothing found.
static Item* FindItemByClassSubclass(Player* player, uint8 itemClass, uint8 itemSubclass)
{
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(NULL_BAG, i);
        if (!item) continue;
        ItemTemplate const* tmpl = item->GetTemplate();
        if (tmpl->Class == itemClass && (itemSubclass == 0 || tmpl->SubClass == itemSubclass))
            return item;
    }
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

static constexpr float  ARRIVE_DIST        = 3.0f;
static constexpr uint32 REVEAL_DELAY_MS    = 900;    // invisible spawn → visible after this ms
static constexpr uint32 DECLINE_DESPAWN_MS = 4000;   // ms before courier despawns after decline

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
        , _revealTimer(0)
        , _emotionDef(nullptr)
        , _talkLooping(false)
        , _talkPhase(false)
        , _closePending(false)
        , _gossipOpenTimer(0)
        , _talkStepTimer(0)
        , _closeTimer(0)
    {}

    void InitializeAI() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);

        Player* player = SummonerPlayer();
        if (!player)
        {
            me->DespawnOrUnsummon(Milliseconds(0));
            return;
        }

        me->SetLevel(player->GetLevel());

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
            case DCP_ARRIVED:     TickArrived(player, diff);     break;
            default:                                              break;
        }
    }

    void HandleGossipHello(Player* player)
    {
        DQ_LOG_INFO(DQ_LOG_CAT_SESSION, player, "GossipHello: phase={}", static_cast<int>(_phase));

        if (_phase != DCP_ARRIVED)
        {
            DQ_LOG_INFO(DQ_LOG_CAT_SESSION, player, "GossipHello: SKIP — not DCP_ARRIVED");
            CloseGossipMenuFor(player);
            return;
        }

        // Cancel pending timer and open immediately
        if (_gossipOpenTimer > 0)
        {
            _gossipOpenTimer = 0;
            OpenGossipNow(player);
            return;
        }

        // Loop already running — re-open gossip without restarting the loop
        if (_talkLooping)
        {
            ReopenGossip(player);
            return;
        }
    }

    bool HandleGossipSelect(Player* player, Creature* creature, uint32 action)
    {
        DQPendingSession& sess = sDQMgr->PendingSession(player);
        if (!DQClientSession::Validate(player, creature, sess))
            return false;
        DQClientSession::Close(sess);

        CloseGossipMenuFor(player);

        // Both paths: mark close pending so loop fires close emote at next step boundary
        _closePending = true;

        if (action == DQ_GOSSIP_DECLINE)
        {
            me->DespawnOrUnsummon(Milliseconds(DECLINE_DESPAWN_MS));
            sDQMgr->OnInteractionDeclined(player);
            return true;
        }

        // Accept path
        _phase = DCP_ACCEPTED;
        me->GetMotionMaster()->Clear();
        me->SetFacingToObject(player);

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
    uint32              _revealTimer;
    const DQEmotionDef* _emotionDef;
    bool                _talkLooping;    // loop is running while gossip is open
    bool                _talkPhase;      // false = play talkEmote next; true = play emotionEmote next
    bool                _closePending;   // player responded; fire close at next step boundary
    uint32              _gossipOpenTimer; // fires after openerEmote duration + 200ms
    uint32              _talkStepTimer;  // countdown for current talk/emotion step
    uint32              _closeTimer;     // countdown after close emote fires (accept path)

    void TickApproaching(Player* player, uint32 /*diff*/)
    {
        if (me->GetDistance(player) > ARRIVE_DIST)
            return;

        me->GetMotionMaster()->Clear();
        me->SetFacingToObject(player);
        _phase = DCP_ARRIVED;

        sDQMgr->OnCourierArrived(player, me);

        if (_emotionDef && _emotionDef->openerEmote != 0)
        {
            me->HandleEmoteCommand(static_cast<Emote>(_emotionDef->openerEmote));
            _gossipOpenTimer = DQEmotionEngine::GetEmoteDuration(_emotionDef->openerEmote) + 200;
        }
        else
        {
            _gossipOpenTimer = 1200;
        }
    }

    void TickArrived(Player* player, uint32 diff)
    {
        // Block A — gossip open timer
        if (_gossipOpenTimer > 0)
        {
            if (_gossipOpenTimer <= diff)
            {
                _gossipOpenTimer = 0;
                OpenGossipNow(player);
            }
            else
                _gossipOpenTimer -= diff;
        }

        // Block B — talk/emotion loop
        if (_talkLooping && _talkStepTimer > 0)
        {
            if (_talkStepTimer <= diff)
            {
                _talkStepTimer = 0;
                if (_closePending)
                {
                    _talkLooping  = false;
                    _closePending = false;
                    if (_emotionDef && _emotionDef->closeEmote != 0)
                    {
                        me->HandleEmoteCommand(static_cast<Emote>(_emotionDef->closeEmote));
                        _closeTimer = DQEmotionEngine::GetEmoteDuration(_emotionDef->closeEmote);
                    }
                }
                else if (_emotionDef)
                {
                    _talkPhase     = !_talkPhase;
                    uint32 emote   = _talkPhase ? _emotionDef->emotionEmote : _emotionDef->talkEmote;
                    if (emote != 0)
                        me->HandleEmoteCommand(static_cast<Emote>(emote));
                    _talkStepTimer = DQEmotionEngine::GetEmoteDuration(emote ? emote : 1);
                }
            }
            else
                _talkStepTimer -= diff;
        }

        // Block C — close timer (accept path; NPC despawned externally by mechanic)
        if (_closeTimer > 0)
        {
            if (_closeTimer <= diff)
                _closeTimer = 0;
            else
                _closeTimer -= diff;
        }
    }

    // Opens gossip, fires talkEmote, starts the loop.
    void OpenGossipNow(Player* player)
    {
        uint32 templateId = sDQMgr->GetActiveTemplateId(player);
        if (!IsArchetypeId(templateId))
            return;

        const InteractionInstance* inst = sDQMgr->GetActiveInst(player);
        if (!inst)
            return;

        uint32 archetypeId           = DecodeArchetypeId(templateId);
        const ArchetypeDef*  def     = sArchetypeMgr->Get(archetypeId);
        const ArchetypeBeat* beat    = sArchetypeMgr->GetBeat(archetypeId, inst->currentPhase);
        if (!def || !beat)
            return;

        DQClientSession::Open(player, me, *def, *beat,
            sDQMgr->PendingSession(player), sDQMgr->cfg_courierTimeoutMs);

        uint32 talkEmote = _emotionDef ? _emotionDef->talkEmote : 0;
        if (talkEmote != 0)
            me->HandleEmoteCommand(static_cast<Emote>(talkEmote));

        _talkStepTimer = DQEmotionEngine::GetEmoteDuration(talkEmote ? talkEmote : 1);
        _talkLooping   = true;
        _talkPhase     = false; // we just fired talk; next toggle → emotionEmote
    }

    // Re-opens the gossip menu without restarting the emote loop.
    void ReopenGossip(Player* player)
    {
        uint32 templateId = sDQMgr->GetActiveTemplateId(player);
        if (!IsArchetypeId(templateId))
        { CloseGossipMenuFor(player); return; }

        const InteractionInstance* inst = sDQMgr->GetActiveInst(player);
        if (!inst)
        { CloseGossipMenuFor(player); return; }

        uint32 archetypeId        = DecodeArchetypeId(templateId);
        const ArchetypeDef*  def  = sArchetypeMgr->Get(archetypeId);
        const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, inst->currentPhase);
        if (def && beat)
            DQClientSession::Open(player, me, *def, *beat,
                sDQMgr->PendingSession(player), sDQMgr->cfg_courierTimeoutMs);
        else
            CloseGossipMenuFor(player);
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
        DQ_LOG_INFO(DQ_LOG_CAT_SESSION, player, "Script::OnGossipHello entry={}", creature->GetEntry());
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
