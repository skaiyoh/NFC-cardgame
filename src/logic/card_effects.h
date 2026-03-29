//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_CARD_EFFECTS_H
#define NFC_CARDGAME_CARD_EFFECTS_H

#include "../data/cards.h"
#include <stdbool.h>

typedef struct GameState GameState;

typedef void (*CardPlayFn)(const Card *card, GameState *state, int playerIndex, int slotIndex);

void card_action_register(const char *type, CardPlayFn fn);

bool card_action_play(const Card *card, GameState *state, int playerIndex, int slotIndex);

void card_action_init(void);

#endif //NFC_CARDGAME_CARD_EFFECTS_H
