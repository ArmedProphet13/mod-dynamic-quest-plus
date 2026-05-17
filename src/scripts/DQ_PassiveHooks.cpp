/*
 * DynamicQuests+ Module
 * Phase 5: Passive Mechanic Detection
 *
 * Two script classes listen globally for game events and route them to
 * DynamicQuestMgr when they involve an active DQ courier:
 *
 *   DQ_UnitHooks     (UnitScript)   -- OnHeal + OnDamage
 *   DQ_SpellCastHook (PlayerScript) -- OnPlayerSpellCast
 *
 * Neither class makes any routing decisions; they are pure event forwarders.
 * All logic lives in DynamicQuestMgr::OnCourier* handlers.
 */

#include "Creature.h"
#include "DynamicQuestMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "Unit.h"

// ---------------------------------------------------------------------------
// DQ_UnitHooks
// ---------------------------------------------------------------------------

class DQ_UnitHooks : public UnitScript
{
public:
    DQ_UnitHooks() : UnitScript("DQ_UnitHooks", true,
    {
        UNITHOOK_ON_HEAL,
        UNITHOOK_ON_DAMAGE,
    }) { }

    void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
    {
        if (!healer || !receiver)
            return;
        if (!healer->IsPlayer() || !receiver->IsCreature())
            return;

        sDQMgr->OnCourierHealed(receiver->GetGUID(), healer->ToPlayer(), gain);
    }

    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        if (!victim || !victim->IsCreature())
            return;

        uint32 health    = victim->GetHealth();
        uint32 maxHealth = victim->GetMaxHealth();
        if (maxHealth == 0)
            return;

        uint32 remainPct = (health > damage)
            ? (health - damage) * 100 / maxHealth
            : 0;

        sDQMgr->OnCourierTookDamage(victim->GetGUID(), remainPct);
    }
};

// ---------------------------------------------------------------------------
// DQ_SpellCastHook
// ---------------------------------------------------------------------------

class DQ_SpellCastHook : public PlayerScript
{
public:
    DQ_SpellCastHook() : PlayerScript("DQ_SpellCastHook",
    {
        PLAYERHOOK_ON_SPELL_CAST,
    }) { }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!player || !spell)
            return;

        sDQMgr->OnPlayerCastOnCourier(player, spell);
    }
};

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void AddSC_DQ_PassiveHooks()
{
    new DQ_UnitHooks();
    new DQ_SpellCastHook();
}
