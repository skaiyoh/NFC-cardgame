//
// Created by Nathan Davis on 2/16/26.
//

#include "entities.h"
#include "../logic/pathfinding.h"
#include "../logic/combat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// TODO: s_nextEntityID is a static global that grows monotonically and never rolls over.
// TODO: At 60fps with many spawns this is practically fine, but it will overflow int after ~2 billion
// TODO: entities. Consider resetting between matches or using a uint32_t with explicit wrap handling.
static int s_nextEntityID = 1;

Entity *entity_create(EntityType type, Faction faction, Vector2 pos) {
    Entity *e = malloc(sizeof(Entity));
    if (!e) return NULL;
    memset(e, 0, sizeof(Entity));

    e->id = s_nextEntityID++;
    e->type = type;
    e->faction = faction;
    e->position = pos;
    e->state = ESTATE_IDLE;
    e->alive = true;
    e->markedForRemoval = false;
    // TODO: spriteScale is hardcoded to 2.0f here; troop_spawn overrides it correctly, but other
    // TODO: entity types that don't override this may inadvertently inherit the wrong scale.
    e->spriteScale = 2.0f;

    anim_state_init(&e->anim, ANIM_IDLE, DIR_UP, 8.0f);

    return e;
}

void entity_destroy(Entity *e) {
    if (!e) return;
    free((char *)e->targetType);
    free(e);
}

void entity_set_state(Entity *e, EntityState newState) {
    if (!e || e->state == newState) return;

    EntityState oldState = e->state;
    SpriteDirection dir = e->anim.dir;
    bool flipH = e->anim.flipH;
    e->state = newState;

    // Reset animation on state change
    switch (newState) {
        case ESTATE_IDLE:
            anim_state_init(&e->anim, ANIM_IDLE, dir, 8.0f);
            break;
        case ESTATE_WALKING:
            anim_state_init(&e->anim, ANIM_WALK, dir, 10.0f);
            break;
        case ESTATE_ATTACKING:
            anim_state_init(&e->anim, ANIM_ATTACK, dir, 10.0f);
            break;
        case ESTATE_DEAD:
            anim_state_init(&e->anim, ANIM_DEATH, dir, 8.0f);
            break;
    }

    e->anim.flipH = flipH;

    // TODO: oldState is discarded — there is no transition-from validation. Any state → any state
    // TODO: is permitted (e.g. DEAD → WALKING). Add guard logic if illegal transitions must be blocked.
    (void) oldState;
}

void entity_update(Entity *e, GameState *gs, float deltaTime) {
    if (!e || !e->alive) return;

    switch (e->state) {
        case ESTATE_IDLE:
            // TODO: ESTATE_IDLE has no combat or targeting behavior. Idle entities should scan for
            // TODO: nearby enemies and transition to ESTATE_WALKING or trigger an attack when in range.
            // Just animate in place
            break;

        case ESTATE_WALKING: {
            Player *owner = &gs->players[e->ownerID];
            pathfind_step_entity(e, owner, deltaTime);

            // Check for enemies in range — transition to attacking
            Entity *target = combat_find_target(e, gs);
            if (target && combat_in_range(e, target, gs)) {
                entity_set_state(e, ESTATE_ATTACKING);
            }
            break;
        }

        case ESTATE_ATTACKING: {
            // Re-acquire target each frame (no cached pointer — avoids stale refs)
            Entity *target = combat_find_target(e, gs);

            if (!target || !combat_in_range(e, target, gs)) {
                entity_set_state(e, ESTATE_WALKING);
                break;
            }

            combat_resolve(e, target, deltaTime);
            break;
        }

        case ESTATE_DEAD:
            // TODO: ESTATE_DEAD immediately marks the entity for removal without letting ANIM_DEATH
            // TODO: play out. entity_set_state sets ANIM_DEATH but the entity is removed next frame.
            // TODO: Wait for the death animation to complete before setting markedForRemoval = true.
            e->markedForRemoval = true;
            break;
    }

    // TODO: anim_state_update ticks unconditionally — dead entities keep animating until removed.
    // TODO: This is harmless since removal happens next frame, but skipping the update for dead
    // TODO: entities would be cleaner.
    anim_state_update(&e->anim, deltaTime);
}

void entity_draw(const Entity *e) {
    if (!e || !e->alive || !e->sprite) return;
    sprite_draw(e->sprite, &e->anim, e->position, e->spriteScale);
}