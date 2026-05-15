/*
 * DynamicQuests+ Module
 * DQ_SuccubusAI — public interface
 * Exposed so MechanicSuccubus can call BeginAcceptSequence / BeginCombat.
 */

#ifndef DQ_SUCCUBUS_AI_H
#define DQ_SUCCUBUS_AI_H

class Player;

class DQ_SuccubusAI
{
public:
    virtual ~DQ_SuccubusAI() = default;

    // Called by MechanicSuccubus::OnChoice(0) — player accepted.
    virtual void BeginAcceptSequence(Player* player) = 0;

    // Called by MechanicSuccubus::OnChoice(1) — player chose to fight.
    virtual void BeginCombat(Player* player) = 0;

    // Called by DQ_SuccubusScript::OnGossipHello — re-open current page.
    virtual void ReopenDialog(Player* player) = 0;
};

#endif // DQ_SUCCUBUS_AI_H
