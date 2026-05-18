/*
 * DynamicQuests+ Module
 * DQ_CourierAI — public interface for the courier NPC AI.
 *
 * Exposes DQCourierBeginDeparture so DQExitSystem can trigger walk-back
 * without needing the full AI class definition.
 */

#ifndef DQ_COURIER_AI_H
#define DQ_COURIER_AI_H

#include "Define.h"
#include "Position.h"

class Creature;

// Instructs the courier AI to begin its departure walk toward dest.
// emoteDuration: milliseconds to wait before starting the walk (lets emote finish).
// No-op if the creature's AI is not DQ_CourierCreatureAI.
void DQCourierBeginDeparture(Creature* courier, const Position& dest, uint32 emoteDuration);

#endif // DQ_COURIER_AI_H
