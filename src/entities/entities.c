//
// Created by Nathan Davis on 2/16/26.
//

#include "entities.h"
#include "entity_animation.h"
#include "../core/battlefield.h"
#include "../core/debug_events.h"
#include "../logic/pathfinding.h"
#include "../logic/combat.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// TODO: s_nextEntityID is a static global that grows monotonically and never rolls over.
// TODO: At 60fps with many spawns this is practically fine, but it will overflow int after ~2 billion
// TODO: entities. Consider resetting between matches or using a uint32_t with explicit wrap handling.
static int s_nextEntityID = 1;

// Orient entity's animation facing toward a target position.
static void entity_face_toward(Entity *e, Vector2 targetPos) {
    float dx = targetPos.x - e->position.x;
    float dy = targetPos.y - e->position.y;
    const float eps = 0.001f;

    if (fabsf(dx) < eps && fabsf(dy) < eps) return;

    if (fabsf(dx) >= eps) {
        e->anim.dir = DIR_SIDE;
        e->anim.flipH = (dx < 0);
    } else if (dy < 0) {
        e->anim.dir = DIR_UP;
        e->anim.flipH = false;
    } else {
        e->anim.dir = DIR_DOWN;
        e->anim.flipH = false;
    }
}

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
    e->attackTargetId = -1;
    // TODO: spriteScale is hardcoded to 2.0f here; troop_spawn overrides it correctly, but other
    // TODO: entity types that don't override this may inadvertently inherit the wrong scale.
    e->spriteScale = 2.0f;
    e->spriteRotationDegrees = 0.0f;

    e->spriteType = SPRITE_TYPE_COUNT; // sentinel: no sprite type assigned yet

    anim_state_init(&e->anim, ANIM_IDLE, DIR_UP, 0.5f, false);

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
    e->hitFlashTimer = 0.0f;

    // Map EntityState → AnimationType for spec lookup
    AnimationType animType;
    switch (newState) {
        case ESTATE_IDLE:      animType = ANIM_IDLE;   break;
        case ESTATE_WALKING:   animType = ANIM_WALK;   break;
        case ESTATE_ATTACKING: animType = ANIM_ATTACK; break;
        case ESTATE_DEAD:      animType = ANIM_DEATH;  break;
        default:               animType = ANIM_IDLE;   break;
    }

    const EntityAnimSpec *spec = anim_spec_get(e->spriteType, animType);
    float duration = spec->cycleSeconds;
    bool oneShot = (spec->mode == ANIM_PLAY_ONCE);

    // Death is always one-shot regardless of spec (prevents fallback-spec death trap)
    if (newState == ESTATE_DEAD) oneShot = true;

    // Stat-driven overrides
    if (newState == ESTATE_WALKING && e->moveSpeed > 0.0f) {
        duration = anim_walk_cycle_seconds(e->moveSpeed, WALK_PIXELS_PER_CYCLE);
    } else if (newState == ESTATE_ATTACKING && e->attackSpeed > 0.0f) {
        duration = anim_attack_cycle_seconds(e->attackSpeed);
    }

    anim_state_init(&e->anim, spec->anim, dir, duration, oneShot);

    // Preserve facing from previous state, unless the new clip locks facing
    // (lockFacing states have facing set explicitly by the caller after this)
    if (!spec->lockFacing) {
        e->anim.flipH = flipH;
    }

    debug_event_emit_xy(e->position.x, e->position.y, DEBUG_EVT_STATE_CHANGE);

    // TODO: oldState is discarded — there is no transition-from validation. Any state → any state
    // TODO: is permitted (e.g. DEAD → WALKING). Add guard logic if illegal transitions must be blocked.
    (void) oldState;
}

void entity_restart_clip(Entity *e) {
    if (!e) return;
    e->anim.elapsed = 0.0f;
    e->anim.normalizedTime = 0.0f;
    e->anim.finished = false;
}

void entity_update(Entity *e, GameState *gs, float deltaTime) {
    if (!e || e->markedForRemoval) return;

    switch (e->state) {
        case ESTATE_IDLE:
            if (!e->alive) break;
            // Idle troops scan for nearby enemies (handles end-of-lane jitter edge case)
            if (e->type == ENTITY_TROOP) {
                Entity *target = combat_find_target(e, gs);
                if (target && combat_in_range(e, target, gs)) {
                    e->attackTargetId = target->id;
                    entity_set_state(e, ESTATE_ATTACKING);
                    entity_face_toward(e, target->position);
                }
            }
            break;

        case ESTATE_WALKING: {
            if (!e->alive) break;
            pathfind_step_entity(e, &gs->battlefield, deltaTime);

            // Check for enemies in range — transition to attacking with target lock
            Entity *target = combat_find_target(e, gs);
            if (target && combat_in_range(e, target, gs)) {
                e->attackTargetId = target->id;
                entity_set_state(e, ESTATE_ATTACKING);
                entity_face_toward(e, target->position);
            }
            break;
        }

        case ESTATE_ATTACKING: {
            if (!e->alive) break;

            // Resolve locked target by ID
            Entity *target = bf_find_entity(&gs->battlefield, e->attackTargetId);

            // Target lost or out of range — transition back to walking
            if (!target || !target->alive || !combat_in_range(e, target, gs)) {
                e->attackTargetId = -1;
                entity_set_state(e, ESTATE_WALKING);
                break;
            }

            // Tick animation and check for hit-marker crossing
            const EntityAnimSpec *spec = anim_spec_get(e->spriteType, ANIM_ATTACK);
            AnimPlaybackEvent evt = anim_state_update(&e->anim, deltaTime);

            if (spec->hitNormalized >= 0.0f &&
                evt.prevNormalized < spec->hitNormalized &&
                evt.currNormalized >= spec->hitNormalized) {
                e->hitFlashTimer = 0.15f;
                // Re-validate target at hit moment
                if (target->alive) {
                    combat_apply_hit(e, target, gs);
                }
            }

            // Tick hit flash timer
            if (e->hitFlashTimer > 0.0f) {
                e->hitFlashTimer -= deltaTime;
            }

            // Clip finished — chain next swing or leave attack
            if (evt.finishedThisTick) {
                // Retarget for next swing
                Entity *nextTarget = combat_find_target(e, gs);
                if (nextTarget && combat_in_range(e, nextTarget, gs)) {
                    e->attackTargetId = nextTarget->id;
                    entity_face_toward(e, nextTarget->position);
                    entity_restart_clip(e);
                } else {
                    e->attackTargetId = -1;
                    entity_set_state(e, ESTATE_WALKING);
                }
            }

            return; // skip unconditional anim_state_update below
        }

        case ESTATE_DEAD: {
            // Death animation plays to completion, then entity is removed
            AnimPlaybackEvent evt = anim_state_update(&e->anim, deltaTime);
            if (evt.finishedThisTick) {
                debug_event_emit_xy(e->position.x, e->position.y, DEBUG_EVT_DEATH_FINISH);
                e->markedForRemoval = true;
            }
            return; // skip the unconditional anim_state_update below
        }
    }

    anim_state_update(&e->anim, deltaTime);
}

void entity_draw(const Entity *e) {
    if (!e || e->markedForRemoval || !e->sprite) return;
    sprite_draw(e->sprite, &e->anim, e->position, e->spriteScale, e->spriteRotationDegrees);
}
