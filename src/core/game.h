//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_GAME_H
#define NFC_CARDGAME_GAME_H

#include "types.h"
#include <stdbool.h>

bool game_init(GameState * g);
void game_update(GameState * g);
void game_render(GameState * g);
void game_cleanup(GameState * g);

#endif //NFC_CARDGAME_GAME_H