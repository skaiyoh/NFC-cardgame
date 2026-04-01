//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_COMBAT_H

#include "../core/types.h"
#include <stdbool.h>

// Returns true if b is within a's attack range (cross-space aware)
bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs);

// Find the best target for attacker among the enemy player's entities
Entity *combat_find_target(Entity *attacker, GameState *gs);

// Apply one attack from attacker to target (respects cooldown)
void combat_resolve(Entity *attacker, Entity *target, float deltaTime);

// Apply damage to an entity; transitions to ESTATE_DEAD if hp <= 0
void entity_take_damage(Entity *entity, int damage);

#endif //NFC_CARDGAME_COMBAT_H
