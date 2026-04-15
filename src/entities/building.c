//
// Created by Nathan Davis on 2/16/26.
//

#include "building.h"
#include "entities.h"
#include "troop.h"
#include "../core/config.h"
#include "../logic/combat.h"
#include "../logic/deposit_slots.h"
#include "../logic/win_condition.h"
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
    e->presentationSide = owner->side;
    e->lane = 1;
    e->laneProgress = 0.0f;
    e->waypointIndex = 0;

    // Sprite
    e->spriteType = SPRITE_TYPE_BASE;
    e->sprite = sprite_atlas_get(atlas, SPRITE_TYPE_BASE);
    e->spriteScale = 2.0f;
    e->bodyRadius = troop_default_body_radius(SPRITE_TYPE_BASE);

    // Navigation: the base is a static blocker whose pathfinding footprint
    // is authored independently of bodyRadius. This keeps combat contact
    // geometry (attackRange vs bodyRadius) unchanged while enlarging the
    // overlap shell so the visible sprite is respected by nav queries.
    e->navProfile = NAV_PROFILE_STATIC;
    e->navRadius = BASE_NAV_RADIUS;

    // Use the front-facing row for both base sprites.
    e->anim.dir = DIR_DOWN;
    e->anim.flipH = false;
    e->spriteRotationDegrees = (owner->side == SIDE_BOTTOM) ? 180.0f : 0.0f;

    // Populate deposit slot ring now that position, navRadius, and
    // presentationSide are finalized.
    deposit_slots_build_for_base(e);

    e->baseLevel = 1;
    e->basePendingKingBurst = false;
    e->basePendingKingBurstDamage = 0;

    printf("[BASE] Spawned base (id=%d) for player %d at (%.0f, %.0f)\n",
           e->id, owner->id, position.x, position.y);

    return e;
}

void building_take_damage(Entity *building, int damage, GameState *gs) {
    if (!building) return;

    bool killed = entity_take_damage(building, damage);
    if (killed && building->type == ENTITY_BUILDING) {
        win_latch_from_destroyed_base(gs, building);
    }
}
