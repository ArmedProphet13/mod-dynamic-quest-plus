/*
 * DynamicQuests+ Module
 * DQ_HungryChildAI — a lost child asking for food (entry 900040)
 *
 * Phase sequence:
 *   HCP_APPROACHING : child walks toward player (MoveFollow).
 *                     EMOTE_STATE_COWER gives persistent distress look during movement.
 *                     EMOTE_ONESHOT_CRY fires every 2.5s over movement frames.
 *                     Transitions when distance ≤ 3y.
 *   HCP_ARRIVED     : child stops, sits, faces player. Gossip opens.
 *                     EMOTE_ONESHOT_CRY fires every 5s while waiting.
 *                     120s inactivity timeout before abort.
 *   HCP_ACCEPTED    : player gave food. Eat animation → laugh → run off.
 *   HCP_DEPARTING   : scheduled DespawnOrUnsummon running.
 *
 * Gossip (always exactly 2 options):
 *   "Here you go, little one."  — only if player has food (CONSUMABLE/FOOD)
 *   "No."                       — always present
 *
 * On accept: remove one food item, deliver reward, play eat → laugh → run off,
 *            then call OnInteractionComplete at the HCP_DEPARTING transition so
 *            the private phase isn't stripped until after the animation plays.
 * On deny:   short cry, despawn after 2.5s, call OnInteractionDeclined.
 * On abort:  immediate despawn, call OnInteractionFailed.
 *
 * Model selection is handled upstream by MechanicHungryChild::GetChildDisplayId,
 * applied to the DQSpawnDesc before the summon enters the world.
 * Item scanning uses DQMechanicUtils::FindFirstItemByClass — covers both the
 * main backpack (slots 23-38) and any extra bags (slots 19-22).
 */

#include "DynamicQuestMgr.h"
#include "MechanicUtils.h"
#include "CreatureScript.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedAI/ScriptedCreature.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "TemporarySummon.h"
#include "ObjectAccessor.h"
#include <cmath>

static constexpr uint32 DQ_HUNGRY_CHILD_SENDER = 1202;
static constexpr uint32 ACTION_GIVE_FOOD       = 0;
static constexpr uint32 ACTION_DENY            = 1;
static constexpr uint32 GOSSIP_TEXT_CHILD      = 9000002;
static constexpr uint32 WAIT_TIMEOUT_MS        = 120000;  // 2 min inactivity abort
static constexpr float  ARRIVE_DIST            = 3.0f;
static const     char*  REWARD_POOL            = "hungry_child_reward";

enum HCPhase : uint8
{
    HCP_APPROACHING = 0,
    HCP_ARRIVED     = 1,
    HCP_ACCEPTED    = 2,
    HCP_DEPARTING   = 3,
};

// ── AI ─────────────────────────────────────────────────────────────────────────

class DQ_HungryChildCreatureAI : public ScriptedAI
{
public:
    explicit DQ_HungryChildCreatureAI(Creature* c)
        : ScriptedAI(c)
        , _phase(HCP_APPROACHING)
        , _waitTimer(WAIT_TIMEOUT_MS)
        , _cryTimer(2500)   // first cry 2.5s after spawn
        , _acceptTimer(0)
        , _laughFired(false)
    {}

    void InitializeAI() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        // Persistent distress state visible between movement steps
        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);

        Player* p = SummonerPlayer();
        if (!p)
        {
            LOG_WARN("module.dynamicquests.hungry_child",
                "HungryChild: InitializeAI — summoner gone at spawn, despawning.");
            me->DespawnOrUnsummon(Milliseconds(0));
            return;
        }
        me->GetMotionMaster()->MoveFollow(p, 1.5f, 0.f);
    }

    void UpdateAI(uint32 diff) override
    {
        Player* player = SummonerPlayer();
        if (!player || !player->IsAlive())
        {
            LOG_WARN("module.dynamicquests.hungry_child",
                "HungryChild: UpdateAI — summoner gone/dead, aborting.");
            Abort();
            return;
        }

        switch (_phase)
        {
            case HCP_APPROACHING: TickApproaching(player, diff); break;
            case HCP_ARRIVED:     TickArrived(player, diff);     break;
            case HCP_ACCEPTED:    TickAccepted(player, diff);    break;
            case HCP_DEPARTING:                                   break;
        }
    }

    void HandleGossipHello(Player* player)
    {
        if (_phase == HCP_ARRIVED)
            OpenGossipMenu(player);
    }

    bool HandleGossipSelect(Player* player, uint32 action)
    {
        CloseGossipMenuFor(player);

        if (action == ACTION_GIVE_FOOD)
        {
            Item* food = DQMechanicUtils::FindFirstItemByClass(
                player, ITEM_CLASS_CONSUMABLE, ITEM_SUBCLASS_FOOD);

            if (!food)
            {
                player->GetSession()->SendAreaTriggerMessage("[DQ+] You don't have any food.");
                OpenGossipMenu(player);
                return true;
            }

            player->DestroyItem(food->GetBagSlot(), food->GetSlot(), true);
            DQMechanicUtils::DeliverReward(player, REWARD_POOL,
                static_cast<uint8>(player->GetLevel()));

            // OnInteractionComplete is called at HCP_DEPARTING (T+3200ms) so the
            // private phase isn't stripped until after the eat → laugh animation plays.
            me->RemoveByteFlag(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_STAND_STATE, UNIT_STAND_STATE_SIT);
            _phase       = HCP_ACCEPTED;
            _acceptTimer = 0;
            _laughFired  = false;

            me->GetMotionMaster()->Clear();
            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);
            me->HandleEmoteCommand(EMOTE_ONESHOT_EAT);

            return true;
        }

        if (action == ACTION_DENY)
        {
            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);
            me->RemoveByteFlag(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_STAND_STATE, UNIT_STAND_STATE_SIT);
            me->HandleEmoteCommand(EMOTE_ONESHOT_CRY);
            me->DespawnOrUnsummon(Milliseconds(2500));
            sDQMgr->OnInteractionDeclined(player);
            return true;
        }

        return false;
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Player* p = SummonerPlayer())
        {
            LOG_WARN("module.dynamicquests.hungry_child",
                "HungryChild: killed for player {}.", p->GetName());
            CloseGossipMenuFor(p);
            sDQMgr->OnInteractionFailed(p);
        }
    }

private:
    HCPhase _phase;
    uint32  _waitTimer;
    uint32  _cryTimer;
    uint32  _acceptTimer;
    bool    _laughFired;

    void TickApproaching(Player* player, uint32 diff)
    {
        // Periodic cry — SMART_ACTION_PLAY_EMOTE equivalent
        if (_cryTimer <= diff)
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_CRY);
            _cryTimer = 2500;
        }
        else
            _cryTimer -= diff;

        if (me->GetDistance(player) <= ARRIVE_DIST)
        {
            me->GetMotionMaster()->Clear();
            me->SetFacingToObject(player);
            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);
            me->SetByteFlag(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_STAND_STATE, UNIT_STAND_STATE_SIT);
            me->HandleEmoteCommand(EMOTE_ONESHOT_CRY);

            _phase     = HCP_ARRIVED;
            _waitTimer = WAIT_TIMEOUT_MS;
            _cryTimer  = 5000;  // periodic cry every 5s while waiting

            sDQMgr->OnCourierArrived(player, me);
        }
    }

    void TickArrived(Player* player, uint32 diff)
    {
        if (_cryTimer <= diff)
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_CRY);
            _cryTimer = 5000;
        }
        else
            _cryTimer -= diff;

        if (_waitTimer <= diff)
            Abort();
        else
            _waitTimer -= diff;
    }

    void TickAccepted(Player* player, uint32 diff)
    {
        _acceptTimer += diff;

        // T+1800ms: laugh
        if (!_laughFired && _acceptTimer >= 1800)
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
            _laughFired = true;
        }

        // T+3200ms: stand up and run away from player, then record completion.
        // Phase is released here — after the animation — so the child stays visible.
        if (_acceptTimer >= 3200 && _phase == HCP_ACCEPTED)
        {
            _phase = HCP_DEPARTING;
            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);
            me->SetWalk(false);

            float angle = player->GetAngle(me);
            float runX  = me->GetPositionX() + 22.0f * std::cos(angle);
            float runY  = me->GetPositionY() + 22.0f * std::sin(angle);
            float runZ  = me->GetPositionZ();
            me->UpdateAllowedPositionZ(runX, runY, runZ);
            me->GetMotionMaster()->MovePoint(1, runX, runY, runZ);
            me->DespawnOrUnsummon(Milliseconds(4000));

            sDQMgr->OnInteractionComplete(player);
        }
    }

    void OpenGossipMenu(Player* player)
    {
        bool hasFood = (DQMechanicUtils::FindFirstItemByClass(
            player, ITEM_CLASS_CONSUMABLE, ITEM_SUBCLASS_FOOD) != nullptr);

        ClearGossipMenuFor(player);
        if (hasFood)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                "Here you go, little one.", DQ_HUNGRY_CHILD_SENDER, ACTION_GIVE_FOOD);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "No.", DQ_HUNGRY_CHILD_SENDER, ACTION_DENY);
        SendGossipMenuFor(player, GOSSIP_TEXT_CHILD, me);
    }

    void Abort()
    {
        me->RemoveByteFlag(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_STAND_STATE, UNIT_STAND_STATE_SIT);
        if (Player* p = SummonerPlayer())
        {
            CloseGossipMenuFor(p);
            sDQMgr->OnInteractionFailed(p);
        }
        me->DespawnOrUnsummon(Milliseconds(0));
    }

    Player* SummonerPlayer()
    {
        if (!me->IsSummon())
            return nullptr;
        WorldObject* s = me->ToTempSummon()->GetSummoner();
        return s ? s->ToPlayer() : nullptr;
    }
};

// ── CreatureScript ─────────────────────────────────────────────────────────────

class DQ_HungryChildScript : public CreatureScript
{
public:
    DQ_HungryChildScript() : CreatureScript("DQ_HungryChildAI") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (DQ_HungryChildCreatureAI* ai = CAST_AI(DQ_HungryChildCreatureAI, creature->AI()))
            ai->HandleGossipHello(player);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
        uint32 /*sender*/, uint32 action) override
    {
        if (DQ_HungryChildCreatureAI* ai = CAST_AI(DQ_HungryChildCreatureAI, creature->AI()))
            ai->HandleGossipSelect(player, action);
        return true;
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new DQ_HungryChildCreatureAI(creature);
    }
};

void AddSC_DQ_HungryChildAI()
{
    new DQ_HungryChildScript();
}
