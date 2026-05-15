/*
 * DynamicQuests+ Module
 * DQ_SuccubusAI — Ileana encounter AI (entry 900020)
 *
 * Phase sequence (all timer-driven via _phaseTimer in UpdateAI):
 *
 *   SBP_RITUAL   (2500ms): Portal GO spawns. Ileana revealed T+1000ms.
 *   SBP_CASTING  (500ms):  Cast Drain Soul Visual (60857) on player — beam + no control.
 *   SBP_DIALOG   (loop):   Force-open gossip. Re-open every 5s if closed.
 *   SBP_ACCEPTED (2300ms): Grovel → yell T+200ms → costs+reward T+2200ms.
 *   SBP_LINGERING(3000ms): Laugh pause before departure.
 *   SBP_DEPARTING(event):  Walk to portal, despawn.
 *   SBP_COMBAT   (loop):   Hostile fight with ability scheduler.
 */

#include "DQ_SuccubusAI.h"
#include "DynamicQuestMgr.h"
#include "DQSpawnSystem.h"
#include "MechanicSuccubus.h"
#include "Chat.h"
#include "CreatureScript.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "InteractionTemplateLibrary.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedAI/ScriptedCreature.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"
#include "GameObject.h"
#include "ObjectAccessor.h"

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr uint32 DQ_SUCCUBUS_SENDER   = 1201;
static constexpr uint32 ACTION_PAGE_NEXT     = 900;
static constexpr uint32 ACTION_ACCEPT        = 0;
static constexpr uint32 ACTION_FIGHT         = 1;

static constexpr uint32 GO_SUMMONING_PORTAL    = 36727; // Summoning Portal GO
static constexpr uint32 SPELL_DRAIN_SOUL_VIS   = 60857; // Drain Soul Visual — beam on player, no damage/control
static constexpr uint32 SPELL_SOUL_DEBT        = 15007; // Resurrection Sickness, 4h
static constexpr int32  SPELL_SOUL_DEBT_MS     = 4 * 3600 * 1000;

static constexpr uint32 SPELL_LASH_OF_PAIN   = 7814;
static constexpr uint32 SPELL_SEDUCTION      = 6358;
static constexpr uint32 SPELL_CORRUPTION     = 172;
static constexpr uint32 SPELL_SHADOW_BOLT    = 9613;
static constexpr uint32 SPELL_DRAIN_LIFE     = 689;
static constexpr uint32 SPELL_SHADOW_WARD    = 29740;

static constexpr uint32 POINT_TO_PORTAL      = 2;
static constexpr float  WALK_STOP_DIST       = 3.0f;

enum SuccubusPhase : uint8
{
    SBP_RITUAL    = 0,
    SBP_WALKING   = 1,
    SBP_CASTING   = 2,
    SBP_DIALOG    = 3,
    SBP_ACCEPTED  = 4,
    SBP_DEPARTING = 5,
    SBP_COMBAT    = 6,
    SBP_LINGERING = 7,
};

// ── Per-episode dialog tables ─────────────────────────────────────────────────

static const uint32 s_pageTextIds[3][4] = {
    { 9001010, 9001011, 9001012,       0 },
    { 9001020, 9001021, 9001022, 9001023 },
    { 9001030, 9001031, 9001032,       0 },
};
static const uint8 s_pageCounts[3] = { 3, 2, 3 };

static const char* s_nextLabels[3][3] = {
    { "What do you want?",        "And what do you get?", nullptr },
    { "What do I have to give?",  nullptr,                nullptr },
    { "I... what do you want?",   "And the price?",       nullptr },
};
static const char* s_acceptLabels[3] = {
    "...done.",
    "Deal.",
    "Take anything. I'm yours.",
};
static const char* s_fightLabels[3] = {
    "Never. I don't deal with demons.",
    "Don't trust demons! Fight.",
    "This time I won't fall for your tricks.",
};

static const char* s_revealLines[3] = {
    "Oh, don't reach for that. We both know how that ends.",
    "Did you miss me?",
    "The third time. I don't come a fourth.",
};
static const char* s_dialogOpenLines[3] = {
    "Let's not waste each other's time.",
    "I know what you want. You know what I want.",
    "No more fractions. No more harmless little trades.",
};
static const char* s_lingerLines[3] = {
    "That wasn't so bad. Was it.",
    "Easy.",
    "Don't worry. You won't remember what you lost.",
};

// ── Portal lifecycle ──────────────────────────────────────────────────────────
// Owns the Summoning Portal GO — spawning, position, and teardown in one place.

struct SBPortal
{
    ObjectGuid guid;
    Position   pos;

    void Spawn(Creature* me, uint32 goEntry)
    {
        if (!guid.IsEmpty())
            return;
        if (GameObject* portal = DQSpawnSystem::SpawnGameObject(
                me, goEntry, pos, me->GetPhaseMask(), 300000))
            guid = portal->GetGUID();
    }

    void Despawn(WorldObject* ref)
    {
        if (guid.IsEmpty())
            return;
        if (GameObject* portal = ObjectAccessor::GetGameObject(*ref, guid))
        {
            portal->SetRespawnTime(0);
            portal->Delete();
        }
        guid = ObjectGuid::Empty;
    }
};

// ── Episode cost application ───────────────────────────────────────────────────
// Applies per-episode economic penalties in one place, separate from phase logic.

struct SBCostApplicator
{
    static void Apply(uint8 episode, Player* player)
    {
        // Gold drain: up to 70% of current gold, capped at (level × 1g)
        uint32 cap    = static_cast<uint32>(player->GetLevel()) * 10000;
        uint32 seventy = static_cast<uint32>(
            static_cast<double>(player->GetMoney()) * 0.70);
        uint32 taken  = std::min(seventy, cap);
        if (taken > 0)
            player->ModifyMoney(-static_cast<int32>(taken));

        // Episode 2+: destroy all potions
        if (episode >= 2)
        {
            for (uint8 bag = INVENTORY_SLOT_BAG_0; bag < INVENTORY_SLOT_BAG_END; ++bag)
                for (uint8 slot = 0; slot < MAX_BAG_SIZE; ++slot)
                {
                    Item* item = player->GetItemByPos(bag, slot);
                    if (!item) continue;
                    const ItemTemplate* t = item->GetTemplate();
                    if (!t) continue;
                    if (t->Class == ITEM_CLASS_CONSUMABLE &&
                        t->SubClass == ITEM_SUBCLASS_POTION)
                        player->DestroyItem(bag, slot, true);
                }
        }

        // Always: drain HP to 20%
        uint32 minHp = std::max<uint32>(1u, player->GetMaxHealth() / 5);
        if (player->GetHealth() > minHp)
            player->SetHealth(minHp);

        // Episode 3: apply 4-hour soul debt aura
        if (episode >= 3)
        {
            if (Aura* debt = player->AddAura(SPELL_SOUL_DEBT, player))
                debt->SetDuration(SPELL_SOUL_DEBT_MS);

            uint32 expiry = static_cast<uint32>(
                GameTime::GetGameTime().count()) + (4 * 3600);
            sDQMgr->SetSoulDebt(player, expiry);

            player->TextEmote(
                "*Something leaves you quietly. You feel lighter. And somehow far less.*");
        }
    }
};

// ── AI class ──────────────────────────────────────────────────────────────────

class DQ_SuccubusCreatureAI : public ScriptedAI, public DQ_SuccubusAI
{
public:
    explicit DQ_SuccubusCreatureAI(Creature* c, uint8 episode)
        : ScriptedAI(c)
        , _episode(episode)
        , _phase(SBP_RITUAL)
        , _phaseTimer(2500)
        , _dialogPage(0)
        , _dialogReopenTimer(5000)
        , _revealed(false)
        , _yellFired(false)
        , _costsFired(false)
        , _wardUsed(false)
        , _lashTimer(8000)
        , _seductionTimer(18000)
        , _corruptionTimer(5000)
        , _shadowBoltTimer(12000)
        , _drainLifeTimer(20000)
    {}

    // ── DQ_SuccubusAI interface ────────────────────────────────────────────

    void BeginAcceptSequence(Player* player) override
    {
        me->InterruptNonMeleeSpells(true);
        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);

        _phase = SBP_ACCEPTED;
        _phaseTimer = 2300;
        _yellFired  = false;
        _costsFired = false;

        player->HandleEmoteCommand(EMOTE_ONESHOT_BEG);
    }

    void BeginCombat(Player* player) override
    {
        player->SetControlled(false, UNIT_STATE_ROOT);
        me->InterruptNonMeleeSpells(true);
        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);
        me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        me->SetFaction(14);

        const InteractionTemplate* tmpl = sInteractionLib->GetById(
            sDQMgr->GetActiveTemplateId(player));
        float hpRatio  = (tmpl && tmpl->combatHpRatio  > 0.f) ? tmpl->combatHpRatio  : 1.8f;
        float dmgRatio = (tmpl && tmpl->combatDmgRatio > 0.f) ? tmpl->combatDmgRatio : 0.07f;

        uint32 hp = static_cast<uint32>(player->GetMaxHealth() * hpRatio);
        me->SetMaxHealth(hp);
        me->SetHealth(hp);

        float dmg = player->GetMaxHealth() * dmgRatio;
        me->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, dmg * 0.85f);
        me->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, dmg * 1.15f);
        me->UpdateAttackPowerAndDamage();
        me->UpdateSpeed(MOVE_RUN, true);

        _phase = SBP_COMBAT;
        me->AI()->AttackStart(player);

        me->Say("If that's what you want.", LANG_UNIVERSAL, player);
    }

    void ReopenDialog(Player* player) override
    {
        if (_phase == SBP_DIALOG)
            ShowPage(player, _dialogPage);
    }

    // ── Core AI hooks ──────────────────────────────────────────────────────

    void InitializeAI() override
    {
        _portal.pos = me->GetPosition();

        me->SetDisplayId(20387); // Illidari Succubus display ID
        me->SetFaction(35);      // friendly during dialog; BeginCombat flips to 14
        me->SetVisible(false);

        me->SetUInt32Value(UNIT_FIELD_FLAGS,
            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
    }

    void UpdateAI(uint32 diff) override
    {
        Player* player = SummonerPlayer();

        if (player && !player->IsAlive())
        {
            Abort();
            return;
        }

        if (_phase == SBP_COMBAT)
        {
            if (!UpdateVictim())
                return;
            TickCombat(diff);
            DoMeleeAttackIfReady();
            return;
        }

        switch (_phase)
        {
            case SBP_RITUAL:    TickRitual(player, diff);    break;
            case SBP_CASTING:   TickCasting(player, diff);   break;
            case SBP_DIALOG:    TickDialog(player, diff);    break;
            case SBP_ACCEPTED:  TickAccepted(player, diff);  break;
            case SBP_LINGERING: TickLingering(player, diff); break;
            default: break;
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id == POINT_TO_PORTAL && _phase == SBP_DEPARTING)
        {
            if (Player* p = SummonerPlayer())
                me->TextEmote(
                    "*She steps back through the portal. The air closes like a wound.*", p);
            _portal.Despawn(me);
            me->DespawnOrUnsummon(Milliseconds(1000));
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        Player* p = SummonerPlayer();
        if (p)
        {
            p->SetControlled(false, UNIT_STATE_ROOT);
            CloseGossipMenuFor(p);
            sDQMgr->OnCourierKilled(p, me);
        }
        _portal.Despawn(me);

        me->TextEmote("*She smiles even as she falls. Like she expected this.*",
            SummonerPlayer());
    }

    void KilledUnit(Unit* victim) override
    {
        if (!victim->IsPlayer())
            return;
        if (Player* p = victim->ToPlayer())
            p->SetControlled(false, UNIT_STATE_ROOT);
        _portal.Despawn(me);
        me->DespawnOrUnsummon(Milliseconds(0));
    }

    bool HandleGossipSelect(Player* player, uint32 action)
    {
        if (action == ACTION_PAGE_NEXT)
        {
            ShowPage(player, _dialogPage + 1);
            return true;
        }
        if (action == ACTION_ACCEPT || action == ACTION_FIGHT)
        {
            CloseGossipMenuFor(player);
            sDQMgr->OnInteractionAccepted(player,
                sDQMgr->GetActiveTemplateId(player), action);
            return true;
        }
        return false;
    }

private:
    uint8         _episode;
    SuccubusPhase _phase;
    uint32        _phaseTimer;
    uint8         _dialogPage;
    uint32        _dialogReopenTimer;
    bool          _revealed;
    bool          _yellFired;
    bool          _costsFired;
    bool          _wardUsed;
    SBPortal      _portal;

    uint32 _lashTimer;
    uint32 _seductionTimer;
    uint32 _corruptionTimer;
    uint32 _shadowBoltTimer;
    uint32 _drainLifeTimer;

    // ── Phase tick implementations ─────────────────────────────────────────

    void TickRitual(Player* player, uint32 diff)
    {
        if (!player) { Abort(); return; }

        if (_portal.guid.IsEmpty())
        {
            float ori  = player->GetOrientation();
            float px   = player->GetPositionX();
            float py   = player->GetPositionY();
            float pz   = player->GetPositionZ();
            float face = ori + static_cast<float>(M_PI);

            _portal.pos.Relocate(
                px + 6.0f * std::cos(ori),
                py + 6.0f * std::sin(ori),
                pz, face);

            me->NearTeleportTo(
                px + WALK_STOP_DIST * std::cos(ori),
                py + WALK_STOP_DIST * std::sin(ori),
                pz, face);

            _portal.Spawn(me, GO_SUMMONING_PORTAL);
        }

        // T+1000ms: reveal
        if (!_revealed && _phaseTimer <= 1500)
        {
            me->SetVisible(true);
            me->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
            me->TextEmote("*She steps through. Laughing softly, like she already knew you'd be here.*", player);
            me->Say(s_revealLines[_episode - 1], LANG_UNIVERSAL, player);
            _revealed = true;
        }

        if (_phaseTimer <= diff)
        {
            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
            me->SetFacingToObject(player);
            _phase = SBP_CASTING;
            _phaseTimer = 500;
        }
        else
            _phaseTimer -= diff;
    }

    void TickCasting(Player* player, uint32 diff)
    {
        if (!player) { Abort(); return; }

        if (_phaseTimer <= diff)
        {
            // Drain Soul Visual (60857): beam on player, no damage, no control.
            // triggered=true bypasses faction check (Ileana is faction 35 = friendly).
            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_SPELL_CHANNEL_DIRECTED);
            me->CastSpell(player, SPELL_DRAIN_SOUL_VIS, true);
            player->SetControlled(true, UNIT_STATE_ROOT); // locked in place; can still click gossip

            sDQMgr->OnCourierArrived(player, me);
            ShowPage(player, 0);
            me->Say(s_dialogOpenLines[_episode - 1], LANG_UNIVERSAL, player);
            _phase = SBP_DIALOG;
        }
        else
            _phaseTimer -= diff;
    }

    void TickDialog(Player* player, uint32 diff)
    {
        if (!player) { Abort(); return; }

        if (_dialogReopenTimer <= diff)
        {
            ShowPage(player, _dialogPage);
            _dialogReopenTimer = 5000;
        }
        else
            _dialogReopenTimer -= diff;
    }

    void TickAccepted(Player* player, uint32 diff)
    {
        if (!player) { Abort(); return; }

        // T+200ms: yell
        if (!_yellFired && _phaseTimer <= 2100)
        {
            me->Yell("Don't worry. I won't take much.", LANG_UNIVERSAL, player);
            _yellFired = true;
        }

        // T+2200ms: drain visual, costs, reward, linger
        if (!_costsFired && _phaseTimer <= 100)
        {
            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_SPELL_CHANNEL_DIRECTED);
            me->CastSpell(player, SPELL_DRAIN_SOUL_VIS, true);

            SBCostApplicator::Apply(_episode, player);
            MechanicSuccubus::DeliverAcceptReward(player, _episode);

            player->SetControlled(false, UNIT_STATE_ROOT);
            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);
            me->InterruptNonMeleeSpells(true);

            sDQMgr->OnInteractionComplete(player);

            me->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
            me->TextEmote("*She laughs — not at you. At the deal. As if she's already spent what she took.*", player);
            me->Say(s_lingerLines[_episode - 1], LANG_UNIVERSAL, player);
            _phase = SBP_LINGERING;
            _phaseTimer = 3000;

            _costsFired = true;
            return;
        }

        _phaseTimer -= diff;
    }

    void TickLingering(Player* player, uint32 diff)
    {
        if (!player) { Abort(); return; }

        if (_phaseTimer <= diff)
        {
            _phase = SBP_DEPARTING;
            me->GetMotionMaster()->Clear();
            me->SetWalk(true);
            me->GetMotionMaster()->MovePoint(POINT_TO_PORTAL,
                _portal.pos.GetPositionX(),
                _portal.pos.GetPositionY(),
                _portal.pos.GetPositionZ());
        }
        else
            _phaseTimer -= diff;
    }

    // ── Dialog page rendering ──────────────────────────────────────────────

    void ShowPage(Player* player, uint8 page)
    {
        uint8 epIdx    = _episode - 1;
        uint8 lastPage = s_pageCounts[epIdx] - 1;
        page = std::min(page, lastPage);

        _dialogPage        = page;
        _dialogReopenTimer = 5000;

        uint32 textId = s_pageTextIds[epIdx][page];

        ClearGossipMenuFor(player);

        if (page < lastPage)
        {
            const char* label = s_nextLabels[epIdx][page];
            if (!label) label = "...";
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label,
                DQ_SUCCUBUS_SENDER, ACTION_PAGE_NEXT);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                s_acceptLabels[epIdx], DQ_SUCCUBUS_SENDER, ACTION_ACCEPT);
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
                s_fightLabels[epIdx],  DQ_SUCCUBUS_SENDER, ACTION_FIGHT);
        }

        SendGossipMenuFor(player, textId, me);
    }

    // ── Combat abilities ───────────────────────────────────────────────────

    void TickCombat(uint32 diff)
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        if (_lashTimer <= diff)
        {
            me->CastSpell(victim, SPELL_LASH_OF_PAIN, false);
            _lashTimer = 8000;
        }
        else _lashTimer -= diff;

        if (_seductionTimer <= diff)
        {
            me->CastSpell(victim, SPELL_SEDUCTION, false);
            _seductionTimer = 18000;
        }
        else _seductionTimer -= diff;

        if (_corruptionTimer <= diff)
        {
            me->CastSpell(victim, SPELL_CORRUPTION, false);
            _corruptionTimer = 15000;
        }
        else _corruptionTimer -= diff;

        if (_shadowBoltTimer <= diff)
        {
            if (me->GetDistance(victim) >= 10.0f)
                me->CastSpell(victim, SPELL_SHADOW_BOLT, false);
            _shadowBoltTimer = 12000;
        }
        else _shadowBoltTimer -= diff;

        if (_episode >= 3)
        {
            if (_drainLifeTimer <= diff)
            {
                me->CastSpell(me, SPELL_DRAIN_LIFE, false);
                me->Say("A little more. Just a taste.", LANG_UNIVERSAL, victim);
                _drainLifeTimer = 20000;
            }
            else _drainLifeTimer -= diff;

            if (!_wardUsed && me->GetHealthPct() <= 50.0f)
            {
                me->CastSpell(me, SPELL_SHADOW_WARD, true);
                me->Say("Did you think it would be that simple?", LANG_UNIVERSAL, victim);
                _wardUsed = true;
            }
        }
    }

    // ── Utilities ──────────────────────────────────────────────────────────

    void Abort()
    {
        _portal.Despawn(me);
        if (Player* p = SummonerPlayer())
        {
            p->SetControlled(false, UNIT_STATE_ROOT);
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

class DQ_SuccubusScript : public CreatureScript
{
public:
    DQ_SuccubusScript() : CreatureScript("DQ_SuccubusAI") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (DQ_SuccubusCreatureAI* ai = CAST_AI(DQ_SuccubusCreatureAI, creature->AI()))
            ai->ReopenDialog(player);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
        uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        if (DQ_SuccubusCreatureAI* ai = CAST_AI(DQ_SuccubusCreatureAI, creature->AI()))
            ai->HandleGossipSelect(player, action);
        return true;
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        uint8 episode = 1;
        if (creature->IsSummon())
        {
            WorldObject* s = creature->ToTempSummon()->GetSummoner();
            if (Player* p = s ? s->ToPlayer() : nullptr)
            {
                uint8 ep = sDQMgr->GetIleanaEpisode(p);
                episode = static_cast<uint8>(ep + 1);
            }
        }
        episode = std::max<uint8>(1, std::min<uint8>(3, episode));
        return new DQ_SuccubusCreatureAI(creature, episode);
    }
};

void AddSC_DQ_SuccubusAI()
{
    new DQ_SuccubusScript();
}
