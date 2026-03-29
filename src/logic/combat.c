//
// Created by Nathan Davis on 2/16/26.
//

#include "combat.h"
#include "../core/types.h"

// TODO: Combat system is completely unimplemented — only forward declarations exist.
// TODO: Entities walk forever and never attack or deal damage. Implement:
// TODO:   combat_in_range()    — return Vector2Distance(a->pos, b->pos) <= a->attackRange
// TODO:   combat_find_target() — iterate the enemy player's entities, apply TroopData.targeting
// TODO:                          (TARGET_NEAREST, TARGET_BUILDING, TARGET_SPECIFIC_TYPE) and
// TODO:                          return the best match, or the enemy base if no troops found
// TODO:   combat_resolve()     — apply attacker->attack to target->hp, call entity_take_damage,
// TODO:                          respect attacker->attackSpeed (track cooldown via attackCooldown field)
// TODO: Call combat_find_target / combat_resolve from entity_update when ESTATE_ATTACKING.
void combat_resolve(Entity * attacker, Entity * target);
bool combat_in_range(Entity * a, Entity * b);
Entity *combat_find_target(Entity * attacker, GameState * gs);