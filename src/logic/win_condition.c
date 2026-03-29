//
// Created by Nathan Davis on 2/16/26.
//

#include "win_condition.h"
#include "../core/types.h"

// TODO: Win condition system is completely unimplemented — only forward declarations exist.
// TODO: The game has no end state: bases are never created (building_create_base returns NULL),
// TODO: so there is nothing to destroy and the match runs forever. Implement:
// TODO:   win_check()   — called each frame; check if either player's base hp <= 0.
// TODO:                   If gs->players[i].base == NULL the check is silently skipped.
// TODO:                   Requires building_create_base to be implemented first.
// TODO:   win_trigger() — set gs->gameOver = true, gs->winnerID = winnerID, display win screen.
// TODO: Call win_check() from game_update() each frame after entity updates.
void win_check(GameState * gs);

void win_trigger(GameState *gs, int winnerID);