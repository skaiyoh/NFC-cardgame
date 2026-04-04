//
// Created by Nathan Davis on 2/16/26.
//

#include "building.h"
#include "entities.h"
#include "../logic/combat.h"
#include <stdio.h>

Entity *building_create_base(Player *owner, Vector2 position, const SpriteAtlas *atlas) {
    Faction faction = (owner->id == 0) ? FACTION_PLAYER1 : FACTION_PLAYER2;
    Entity *e = entity_create(ENTITY_BUILDING, faction, position);
    if (!e) return NULL;

    // Stats: high HP, no attack, stationary
    e->hp = 5000;
    e->maxHP = 5000;
    e->attack = 0;
    e->attackSpeed = 0.0f;
    e->attackRange = 0.0f;
    e->moveSpeed = 0.0f;

    // Targeting (irrelevant — base never attacks)
    e->targeting = TARGET_NEAREST;

    // Ownership / lane
    e->ownerID = owner->id;
    e->lane = 1;
    e->waypointIndex = 0;

    // Sprite
    e->spriteType = SPRITE_TYPE_BASE;
    e->sprite = sprite_atlas_get(atlas, SPRITE_TYPE_BASE);
    e->spriteScale = 3.0f;

    // Face toward enemy
    e->anim.dir = (owner->side == SIDE_BOTTOM) ? DIR_UP : DIR_DOWN;
    e->anim.flipH = false;

    printf("[BASE] Spawned base (id=%d) for player %d at (%.0f, %.0f)\n",
           e->id, owner->id, position.x, position.y);

    return e;
}

void building_take_damage(Entity *building, int damage) {
    entity_take_damage(building, damage);
}
