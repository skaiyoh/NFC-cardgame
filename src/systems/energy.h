//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_ENERGY_H
#define NFC_CARDGAME_ENERGY_H

#include <stdbool.h>

// Forward declaration — avoid circular includes with types.h
typedef struct Player Player;

void energy_init(Player *p, float maxEnergy, float regenRate);

void energy_update(Player *p, float deltaTime);

bool energy_can_afford(Player *p, int cost);

bool energy_consume(Player *p, int cost);

void energy_restore(Player *p, float amount);

void energy_set_regen_rate(Player *p, float newRate);

#endif //NFC_CARDGAME_ENERGY_H