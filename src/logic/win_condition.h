//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_WIN_CONDTION_H
#define NFC_CARDGAME_WIN_CONDTION_H

#include "../core/types.h"

// Idempotent latch: sets gameOver and winnerID. No-op if already latched.
void win_trigger(GameState *gs, int winnerID);

// Authoritative latch for base destruction. Call after any lethal base hit.
// No-op if gameOver already set or if destroyedBase is not a player's base.
void win_latch_from_destroyed_base(GameState *gs, const Entity *destroyedBase);

// Defensive fallback: scans player bases for death and latches a winner if
// exactly one base is dead, or a draw if both are already dead.
// Primary win detection is in the damage path via win_latch_from_destroyed_base.
void win_check(GameState *gs);

#endif //NFC_CARDGAME_WIN_CONDTION_H
