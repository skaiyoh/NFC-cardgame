//
// Created by Nathan Davis on 2/16/26.
//

#include "building.h"

// TODO: building_create_base is not implemented — it always returns NULL.
// TODO: Player.base is therefore always NULL, making the win condition system unable to check for
// TODO: base destruction. Implement base entity creation using entity_create(ENTITY_BUILDING, ...).
// TODO: Call this from player_init and assign the result to p->base.
Entity *building_create_base(Player *owner, Vector2 position) {
    (void) owner;
    (void) position;
    return NULL;
}

void building_take_damage(Entity *building, int damage) {
    (void) building;
    (void) damage;
}