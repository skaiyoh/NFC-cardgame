//
// Created by Nathan Davis on 2/16/26.
//

#include "win_condition.h"
#include <stdio.h>

static void win_trigger_draw(GameState *gs) {
    if (!gs || gs->gameOver) return;

    gs->gameOver = true;
    gs->winnerID = -1;

    printf("[WIN] Match drawn!\n");
}

void win_trigger(GameState *gs, int winnerID) {
    if (!gs || gs->gameOver) return;

    gs->gameOver = true;
    gs->winnerID = winnerID;

    printf("[WIN] Player %d wins!\n", winnerID);
}

void win_latch_from_destroyed_base(GameState *gs, const Entity *destroyedBase) {
    if (!gs || !destroyedBase || gs->gameOver) return;

    for (int i = 0; i < 2; i++) {
        if (gs->players[i].base == destroyedBase) {
            win_trigger(gs, 1 - i);
            return;
        }
    }
}

void win_check(GameState *gs) {
    if (!gs || gs->gameOver) return;

    int deadBaseCount = 0;
    int deadBaseOwner = -1;

    for (int i = 0; i < 2; i++) {
        Entity *base = gs->players[i].base;
        if (!base) continue;

        if (!base->alive || base->hp <= 0) {
            deadBaseCount++;
            deadBaseOwner = i;
        }
    }

    if (deadBaseCount == 1) {
        printf("[WIN] WARNING: primary latch bypassed, fallback caught dead base for player %d\n", deadBaseOwner);
        win_trigger(gs, 1 - deadBaseOwner);
        return;
    }

    if (deadBaseCount > 1) {
        printf("[WIN] WARNING: primary latch bypassed, fallback found both player bases dead\n");
        win_trigger_draw(gs);
    }
}
