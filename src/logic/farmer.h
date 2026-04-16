//
// Farmer unit behavior -- seek/gather/return/deposit sustenance loop.
//

#ifndef NFC_CARDGAME_FARMER_H
#define NFC_CARDGAME_FARMER_H

#include "../core/types.h"

// Run one frame of the farmer state machine.
// Called from entity_update() instead of the combat path.
void farmer_update(Entity *e, GameState *gs, float deltaTime);

// Handle farmer death: release sustenance claim, award carried sustenance to opponent.
// Idempotent -- safe to call from multiple kill paths.
void farmer_on_death(Entity *farmer, GameState *gs);

// Resolve the farmer's current free-goal steering target without mutating
// claims, reservations, or entity state. Used by nav debug overlays.
bool farmer_debug_nav_goal(const Entity *e, const GameState *gs,
                           Vector2 *outGoal, float *outStopRadius);

#endif //NFC_CARDGAME_FARMER_H
