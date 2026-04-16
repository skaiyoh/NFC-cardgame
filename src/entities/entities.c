//
// Created by Nathan Davis on 2/16/26.
//

#include "entities.h"
#include "entity_animation.h"
#include "projectile.h"
#include "../core/battlefield.h"
#include "../core/config.h"
#include "../core/debug_events.h"
#include "../logic/pathfinding.h"
#include "../logic/combat.h"
#include "../logic/farmer.h"
#include "../systems/progression.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// TODO: s_nextEntityID is a static global that grows monotonically and never rolls over.
// TODO: At 60fps with many spawns this is practically fine, but it will overflow int after ~2 billion
// TODO: entities. Consider resetting between matches or using a uint32_t with explicit wrap handling.
static int s_nextEntityID = 1;

// Orient entity's animation facing toward a target position.
static void entity_face_toward(Entity *e, const Battlefield *bf, Vector2 targetPos) {
    pathfind_commit_presentation(e, bf);
    pathfind_face_goal(e, bf, targetPos);
}

static bool entity_target_uses_static_assault_cloud(const Entity *target) {
    if (!target) return false;
    return target->type == ENTITY_BUILDING || target->navProfile == NAV_PROFILE_STATIC;
}

static void entity_apply_enemy_pursuit(Entity *e, GameState *gs, Entity *pursuit) {
    if (!e || !gs) return;

    if (!pursuit) {
        e->movementTargetId = -1;
        e->lastSteerSideSign = 0;
        if (e->unitRole == UNIT_ROLE_COMBAT) {
            e->navProfile = NAV_PROFILE_LANE;
        }
        return;
    }

    bool targetChanged = (e->movementTargetId != pursuit->id);
    e->movementTargetId = pursuit->id;
    if (targetChanged) {
        e->lastSteerSideSign = 0;
    }
    if (e->unitRole == UNIT_ROLE_COMBAT) {
        e->navProfile = NAV_PROFILE_ASSAULT;
    }
}

static Entity *entity_find_enemy_base_objective(Entity *e, GameState *gs) {
    if (!e || !gs) return NULL;

    int enemyPlayerId = (e->ownerID == 0) ? 1 : 0;
    Entity *base = gs->players[enemyPlayerId].base;
    if (!base || !base->alive || base->markedForRemoval) return NULL;
    if (base->ownerID == e->ownerID) return NULL;
    return base;
}

static bool entity_lane_march_exhausted(Entity *e, Battlefield *bf) {
    if (!e || !bf) return false;
    if (e->lane < 0 || e->lane >= 3) return false;

    if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
        return true;
    }

    pathfind_sync_lane_progress(e, bf);
    return e->waypointIndex >= LANE_WAYPOINT_COUNT;
}

static Entity *entity_find_base_objective_fallback(Entity *e, GameState *gs) {
    if (!e || !gs) return NULL;
    if (e->unitRole != UNIT_ROLE_COMBAT) return NULL;
    if (!entity_lane_march_exhausted(e, &gs->battlefield)) return NULL;
    return entity_find_enemy_base_objective(e, gs);
}

// Enemy-only validation for local-steering pursuit targets.
// Healers should not pursue injured allies across the map, so movementTargetId
// is always restricted to enemy entities even though combat_find_target() can
// return a friendly heal target when one is already in attack range.
static Entity *entity_validate_enemy_pursuit(Entity *e, const Entity *candidate,
                                             Battlefield *bf, float maxRadius) {
    if (!e || !candidate) return NULL;
    if (!bf) return NULL;
    if (!candidate->alive || candidate->markedForRemoval) return NULL;
    if (candidate->ownerID == e->ownerID) return NULL;
    if (maxRadius < 0.0f) return NULL;

    if (e->lane >= 0 && e->lane < 3) {
        pathfind_sync_lane_progress(e, bf);
        float targetProgress = pathfind_lane_progress_for_position(e, bf, candidate->position);
        if (targetProgress + PATHFIND_PURSUIT_REAR_TOLERANCE < e->laneProgress) {
            return NULL;
        }
    }

    float dx = candidate->position.x - e->position.x;
    float dy = candidate->position.y - e->position.y;
    if (dx * dx + dy * dy > maxRadius * maxRadius) return NULL;
    return (Entity *)candidate;
}

static Entity *entity_validate_enemy_pursuit_id(Entity *e, Battlefield *bf,
                                                int targetId, float maxRadius) {
    if (!e || !bf || targetId < 0) return NULL;
    return entity_validate_enemy_pursuit(e, bf_find_entity(bf, targetId), bf, maxRadius);
}

static bool entity_should_seed_pursuit_for_target(Entity *e, GameState *gs,
                                                   Entity *target) {
    if (!e || !gs || !target) return false;
    if (target->ownerID == e->ownerID) return false;
    if (entity_target_uses_static_assault_cloud(target)) return true;
    return entity_validate_enemy_pursuit(
        e, target, &gs->battlefield,
        PATHFIND_AGGRO_RADIUS + PATHFIND_AGGRO_HYSTERESIS
    ) != NULL;
}

static Entity *entity_find_forward_pursuit_target(Entity *e, GameState *gs, float maxRadius) {
    if (!e || !gs) return NULL;
    if (maxRadius < 0.0f) return NULL;

    Battlefield *bf = &gs->battlefield;
    Entity *bestTarget = NULL;
    float bestDistSq = INFINITY;

    for (int i = 0; i < bf->entityCount; i++) {
        Entity *candidate = entity_validate_enemy_pursuit(e, bf->entities[i], bf, maxRadius);
        if (!candidate) continue;

        float dx = candidate->position.x - e->position.x;
        float dy = candidate->position.y - e->position.y;
        float distSq = dx * dx + dy * dy;

        switch (e->targeting) {
            case TARGET_BUILDING:
                if (candidate->type == ENTITY_BUILDING) {
                    if (distSq < bestDistSq || (bestTarget && bestTarget->type != ENTITY_BUILDING)) {
                        bestTarget = candidate;
                        bestDistSq = distSq;
                    }
                    continue;
                }
                if (bestTarget && bestTarget->type == ENTITY_BUILDING) {
                    continue;
                }
                // fallthrough
            case TARGET_NEAREST:
            case TARGET_SPECIFIC_TYPE:
                if (!bestTarget || distSq < bestDistSq) {
                    bestTarget = candidate;
                    bestDistSq = distSq;
                }
                break;
        }
    }

    return bestTarget;
}

static Entity *entity_select_post_attack_pursuit(Entity *e, GameState *gs,
                                                 Entity *staleTarget) {
    if (!e || !gs) return NULL;

    Entity *pursuit = entity_validate_enemy_pursuit(
        e, staleTarget, &gs->battlefield,
        PATHFIND_AGGRO_RADIUS + PATHFIND_AGGRO_HYSTERESIS
    );
    if (pursuit) return pursuit;

    pursuit = entity_find_forward_pursuit_target(e, gs, PATHFIND_AGGRO_RADIUS);
    if (pursuit) return pursuit;

    return entity_find_base_objective_fallback(e, gs);
}

// Refresh the current movementTargetId using the standard aggro policy:
// keep a valid existing enemy target out to AGGRO + HYSTERESIS, otherwise
// acquire the nearest valid forward-biased enemy within AGGRO. Once the troop
// has exhausted lane marching, fall back to the enemy base so temporary edge
// chases cannot strand it in idle.
static Entity *entity_refresh_enemy_pursuit(Entity *e, GameState *gs) {
    if (!e || !gs) return NULL;

    Battlefield *bf = &gs->battlefield;
    Entity *pursuit = entity_validate_enemy_pursuit_id(
        e, bf, e->movementTargetId,
        PATHFIND_AGGRO_RADIUS + PATHFIND_AGGRO_HYSTERESIS
    );
    if (pursuit) {
        entity_apply_enemy_pursuit(e, gs, pursuit);
        return pursuit;
    }

    pursuit = entity_find_forward_pursuit_target(e, gs, PATHFIND_AGGRO_RADIUS);
    if (!pursuit) {
        pursuit = entity_find_base_objective_fallback(e, gs);
    }
    entity_apply_enemy_pursuit(e, gs, pursuit);
    return pursuit;
}

static bool entity_attack_uses_projectile_reservation(const Entity *e) {
    if (!e) return false;
    return e->type == ENTITY_TROOP &&
           e->deliveryMode == ATTACK_DELIVERY_PROJECTILE &&
           e->healAmount <= 0;
}

static bool entity_attack_uses_windup_commit(const Entity *e) {
    if (!e) return false;
    if (e->type != ENTITY_TROOP) return false;
    return e->deliveryMode == ATTACK_DELIVERY_INSTANT ||
           entity_attack_uses_projectile_reservation(e);
}

static void entity_release_reserved_projectile(Entity *e, GameState *gs) {
    if (!e || !gs) return;
    if (e->reservedProjectileSlotIndex < 0) return;

    projectile_release_slot(gs, e->reservedProjectileSlotIndex);
    e->reservedProjectileSlotIndex = -1;
}

static bool entity_ensure_reserved_projectile(Entity *e, GameState *gs) {
    if (!e || !gs) return false;
    if (!entity_attack_uses_projectile_reservation(e)) return true;
    if (e->reservedProjectileSlotIndex >= 0) return true;

    int slotIndex = projectile_reserve_slot(gs);
    if (slotIndex < 0) return false;

    e->reservedProjectileSlotIndex = slotIndex;
    return true;
}

static void entity_sync_reserved_projectile_state(Entity *e, GameState *gs) {
    if (!e || !gs) return;
    if (e->reservedProjectileSlotIndex < 0) return;
    if (e->state == ESTATE_ATTACKING && e->alive && !e->markedForRemoval) return;

    entity_release_reserved_projectile(e, gs);
}

static bool entity_begin_attack(Entity *e, GameState *gs, Entity *target,
                                bool restartClip) {
    if (!e || !gs || !target) return false;

    if (!entity_attack_uses_projectile_reservation(e)) {
        entity_release_reserved_projectile(e, gs);
    } else if (!entity_ensure_reserved_projectile(e, gs)) {
        return false;
    }

    if (entity_should_seed_pursuit_for_target(e, gs, target)) {
        entity_apply_enemy_pursuit(e, gs, target);
    }
    e->attackTargetId = target->id;
    if (restartClip) {
        entity_face_toward(e, &gs->battlefield, target->position);
        entity_restart_clip(e);
    } else {
        entity_set_state(e, ESTATE_ATTACKING);
        entity_face_toward(e, &gs->battlefield, target->position);
    }
    return true;
}

static Entity *entity_retarget_or_walk(Entity *e, GameState *gs) {
    Entity *nextTarget = combat_find_target(e, gs);
    if (nextTarget && combat_in_range(e, nextTarget, gs) &&
        entity_begin_attack(e, gs, nextTarget, true)) {
        return nextTarget;
    }

    // No in-range target: immediate pursuit from the stale attackTargetId so
    // local steering has a goal before the next tick instead of lane-drifting
    // for one frame. Use the same release radius as the walking hysteresis.
    Entity *stale = (e->attackTargetId != -1)
        ? bf_find_entity(&gs->battlefield, e->attackTargetId) : NULL;
    Entity *pursuit = entity_select_post_attack_pursuit(e, gs, stale);
    entity_apply_enemy_pursuit(e, gs, pursuit);
    entity_release_reserved_projectile(e, gs);
    e->attackTargetId = -1;
    entity_set_state(e, ESTATE_WALKING);
    return NULL;
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
    e->attackReleaseFired = false;
    e->attackWindupCommitted = false;
    e->reservedProjectileSlotIndex = -1;
    e->movementTargetId = -1;
    e->ticksSinceProgress = 0;
    e->laneProgress = 0.0f;
    e->bodyRadius = 14.0f;  // sensible default; overridden by troop/building spawn paths
    e->navRadius = 0.0f;    // 0 => pathfinding falls back to bodyRadius
    e->navProfile = NAV_PROFILE_LANE; // overridden by troop_spawn / building_create_base
    e->reservedDepositSlotIndex = -1;
    e->reservedDepositSlotKind = DEPOSIT_SLOT_NONE;
    e->lastSteerSideSign = 0;
    // TODO: spriteScale is hardcoded to 2.0f here; troop_spawn overrides it correctly, but other
    // TODO: entity types that don't override this may inadvertently inherit the wrong scale.
    e->spriteScale = 2.0f;
    e->spriteRotationDegrees = 0.0f;
    e->presentationSide = SIDE_BOTTOM;
    e->renderLayer = ENTITY_RENDER_LAYER_GROUND;

    e->spriteType = SPRITE_TYPE_COUNT; // sentinel: no sprite type assigned yet
    e->combatProfileId = COMBAT_PROFILE_DEFAULT_MELEE;
    e->engagementMode = ATTACK_ENGAGEMENT_CONTACT;
    e->deliveryMode = ATTACK_DELIVERY_INSTANT;
    e->projectileVisualType = PROJECTILE_VISUAL_NONE;
    e->projectileSpeed = 0.0f;
    e->projectileHitRadius = 0.0f;
    e->projectileSplashRadius = 0.0f;
    e->projectileRenderScale = 1.0f;
    e->projectileLaunchOffset = (Vector2){ 0.0f, 0.0f };

    // Default to combat role; farmer overrides in troop_spawn
    e->unitRole = UNIT_ROLE_COMBAT;
    e->farmerState = FARMER_SEEKING;
    e->claimedSustenanceNodeId = -1;
    e->carriedSustenanceValue = 0;
    e->workTimer = 0.0f;

    anim_state_init(&e->anim, ANIM_IDLE, DIR_UP, 0.5f, false);

    return e;
}

void entity_destroy(Entity *e) {
    if (!e) return;
    free((char *)e->targetType);
    free(e);
}

static AnimationType entity_anim_type_for_state(EntityState state) {
    // Map EntityState → AnimationType for spec lookup
    switch (state) {
        case ESTATE_IDLE:      return ANIM_IDLE;
        case ESTATE_WALKING:   return ANIM_WALK;
        case ESTATE_ATTACKING: return ANIM_ATTACK;
        case ESTATE_DEAD:      return ANIM_DEATH;
        default:               return ANIM_IDLE;
    }
}

static unsigned int entity_anim_seed(const Entity *e, AnimationType animType) {
    unsigned int seed = (unsigned int) (e ? (e->id + 1) : 1) * 0x9e3779b9u;
    seed ^= (unsigned int) ((e ? e->spriteType : 0) + 1) * 0x85ebca6bu;
    seed ^= (unsigned int) ((e ? e->ownerID : 0) + 1) * 0xc2b2ae35u;
    seed ^= (unsigned int) (animType + 1) * 0x27d4eb2du;
    return seed;
}

static void entity_sync_attack_commit_progress(Entity *e) {
    if (!entity_attack_uses_windup_commit(e)) return;
    if (e->state != ESTATE_ATTACKING) return;
    if (e->attackWindupCommitted) return;

    if (e->anim.normalizedTime > 0.0f || e->anim.finished) {
        e->attackWindupCommitted = true;
    }
}

static void entity_note_attack_progress(Entity *e, const AnimPlaybackEvent *evt) {
    if (!entity_attack_uses_windup_commit(e) || !evt) return;
    if (e->state != ESTATE_ATTACKING) return;
    if (e->attackWindupCommitted) return;

    if (evt->currNormalized > 0.0f || evt->finishedThisTick) {
        e->attackWindupCommitted = true;
    }
}

void entity_sync_animation(Entity *e) {
    if (!e) return;

    SpriteDirection dir = e->anim.dir;
    bool flipH = e->anim.flipH;
    AnimationType animType = entity_anim_type_for_state(e->state);

    const EntityAnimSpec *spec = anim_spec_get(e->spriteType, animType);
    float duration = spec->cycleSeconds;
    bool oneShot = (spec->mode == ANIM_PLAY_ONCE);
    int visualLoops = (spec->visualLoops > 0) ? spec->visualLoops : 1;

    // Death is always one-shot regardless of spec (prevents fallback-spec death trap)
    if (e->state == ESTATE_DEAD) oneShot = true;

    // Stat-driven overrides
    if (e->state == ESTATE_WALKING && e->moveSpeed > 0.0f) {
        duration = anim_walk_cycle_seconds(e->moveSpeed, WALK_PIXELS_PER_CYCLE);
    } else if (e->state == ESTATE_ATTACKING && e->attackSpeed > 0.0f) {
        duration = anim_attack_cycle_seconds(e->attackSpeed);
    }

    if (spec->mode == ANIM_PLAY_IDLE_BURST) {
        anim_state_init_idle_burst(&e->anim, spec->anim, dir, duration,
                                   spec->idleHoldMinSeconds, spec->idleHoldMaxSeconds,
                                   spec->idleInitialPhaseNormalized,
                                   visualLoops, entity_anim_seed(e, animType));
    } else {
        anim_state_init_with_loops(&e->anim, spec->anim, dir, duration, oneShot, visualLoops);
    }

    // Preserve facing from previous state, unless the new clip locks facing
    // (lockFacing states have facing set explicitly by the caller after this)
    if (!spec->lockFacing) {
        e->anim.flipH = flipH;
    }
}

void entity_set_state(Entity *e, EntityState newState) {
    if (!e || e->state == newState) return;

    EntityState oldState = e->state;
    e->state = newState;
    e->hitFlashTimer = 0.0f;
    e->attackReleaseFired = false;
    e->attackWindupCommitted = false;

    entity_sync_animation(e);

    debug_event_emit_xy(e->position.x, e->position.y, DEBUG_EVT_STATE_CHANGE);

    // TODO: oldState is discarded — there is no transition-from validation. Any state → any state
    // TODO: is permitted (e.g. DEAD → WALKING). Add guard logic if illegal transitions must be blocked.
    (void) oldState;
}

void entity_restart_clip(Entity *e) {
    if (!e) return;
    e->attackReleaseFired = false;
    e->attackWindupCommitted = false;
    anim_state_restart(&e->anim);
}

void entity_update(Entity *e, GameState *gs, float deltaTime) {
    if (!e) return;
    if (gs) {
        entity_sync_reserved_projectile_state(e, gs);
    }
    if (!gs || e->markedForRemoval) return;

    // TODO: Farmer bypasses combat state machine. Add ESTATE_WORKING when
    // farmer-specific animations are available.
    if (e->unitRole == UNIT_ROLE_FARMER) {
        farmer_update(e, gs, deltaTime);
        return;
    }

    switch (e->state) {
        case ESTATE_IDLE:
            if (!e->alive) break;
            // First honor the normal in-range attack/heal semantics.
            if (e->type == ENTITY_TROOP) {
                Entity *target = combat_find_target(e, gs);
                if (target && combat_in_range(e, target, gs) &&
                    entity_begin_attack(e, gs, target, false)) {
                    break;
                }

                // Then probe enemy-only pursuit so lane-end/idling units can
                // chase nearby enemies that are not yet in attack range.
                Entity *pursuit = entity_refresh_enemy_pursuit(e, gs);
                if (pursuit) {
                    entity_set_state(e, ESTATE_WALKING);
                    entity_face_toward(e, &gs->battlefield, pursuit->position);
                }
            }
            break;

        case ESTATE_WALKING: {
            if (!e->alive) break;
            Battlefield *bf = &gs->battlefield;

            // (a) Refresh the enemy-only pursuit target. In-range healing
            // stays in the post-step combat_find_target() below.
            Entity *pursuit = entity_refresh_enemy_pursuit(e, gs);

            // (b) Probe-before-move: if the pursuit target is already in
            // attack range, transition this tick instead of lane-walking
            // one more step past it.
            bool attackStartBlocked = false;
            if (pursuit && combat_in_range(e, pursuit, gs)) {
                if (entity_begin_attack(e, gs, pursuit, false)) {
                    break;
                }
                attackStartBlocked = true;
            }

            // (c) Step. Phase 2 still uses waypoint stepping; Phase 3 makes
            // pathfind_step_entity goal-aware so it steers toward the
            // movementTarget when one is set.
            pathfind_step_entity(e, &gs->nav, bf, deltaTime);

            // (d) Post-step attack check -- uses the heal-first combat_find_target
            // so healers can still heal an injured ally that just landed in
            // range after the walk step.
            if (!attackStartBlocked) {
                Entity *target = combat_find_target(e, gs);
                if (target && combat_in_range(e, target, gs)) {
                    entity_begin_attack(e, gs, target, false);
                }
            }
            break;
        }

        case ESTATE_ATTACKING: {
            if (!e->alive) break;

            // Buildings play a one-shot attack clip. Bases queue a King burst
            // via play_king and resolve damage when the clip crosses the spec's
            // hit marker. Any pending burst is cleared when the clip finishes
            // so a whiff still ends cleanly.
            if (e->type == ENTITY_BUILDING) {
                const EntityAnimSpec *spec = anim_spec_get(e->spriteType, ANIM_ATTACK);
                AnimPlaybackEvent evt = anim_state_update(&e->anim, deltaTime);

                if (spec->hitNormalized >= 0.0f &&
                    evt.prevNormalized < spec->hitNormalized &&
                    evt.currNormalized >= spec->hitNormalized &&
                    e->basePendingKingBurst) {
                    combat_apply_king_burst(e, PROGRESSION_KING_BURST_RADIUS,
                                            e->basePendingKingBurstDamage, gs);
                    e->basePendingKingBurst = false;
                    e->basePendingKingBurstDamage = 0;
                }

                if (evt.finishedThisTick) {
                    e->basePendingKingBurst = false;
                    e->basePendingKingBurstDamage = 0;
                    entity_set_state(e, ESTATE_IDLE);
                }
                return;
            }

            // Resolve locked target by ID
            Entity *target = bf_find_entity(&gs->battlefield, e->attackTargetId);
            entity_sync_attack_commit_progress(e);
            bool usesProjectileReservation = entity_attack_uses_projectile_reservation(e);
            bool committedWindup = entity_attack_uses_windup_commit(e) &&
                                   e->attackWindupCommitted;
            bool targetAvailable = target && target->alive && !target->markedForRemoval;
            bool targetInRange = targetAvailable && combat_in_range(e, target, gs);
            bool cancelForUnavailableTarget = !targetAvailable &&
                (!committedWindup || usesProjectileReservation);
            bool cancelForRangeLoss = targetAvailable && !targetInRange &&
                                      !committedWindup;

            // Melee and offensive projectile attacks can commit their wind-up
            // once the clip visibly advances. Offensive projectile attacks
            // still require the locked target to remain alive until release.
            if (cancelForUnavailableTarget || cancelForRangeLoss) {
                Entity *pursuit = entity_select_post_attack_pursuit(e, gs, target);
                entity_apply_enemy_pursuit(e, gs, pursuit);
                entity_release_reserved_projectile(e, gs);
                e->attackTargetId = -1;
                entity_set_state(e, ESTATE_WALKING);
                break;
            }

            if (e->healAmount > 0 &&
                target->ownerID == e->ownerID &&
                !combat_can_heal_target(e, target)) {
                target = entity_retarget_or_walk(e, gs);
                if (!target) break;
            }

            // Tick animation and check for hit-marker crossing
            const EntityAnimSpec *spec = anim_spec_get(e->spriteType, ANIM_ATTACK);
            AnimPlaybackEvent evt = anim_state_update(&e->anim, deltaTime);
            entity_note_attack_progress(e, &evt);

            if (spec->hitNormalized >= 0.0f &&
                evt.prevNormalized < spec->hitNormalized &&
                evt.currNormalized >= spec->hitNormalized) {
                e->hitFlashTimer = 0.15f;
                if (!e->attackReleaseFired) {
                    if (e->deliveryMode == ATTACK_DELIVERY_PROJECTILE) {
                        if (usesProjectileReservation) {
                            if (targetAvailable &&
                                projectile_activate_reserved_attack(
                                    gs, e->reservedProjectileSlotIndex, e, target)) {
                                e->reservedProjectileSlotIndex = -1;
                                e->attackReleaseFired = true;
                            } else {
                                entity_release_reserved_projectile(e, gs);
                            }
                        } else if (targetAvailable) {
                            if (!projectile_spawn_for_attack(gs, e, target)) {
                                // Legacy support/projectile fallback: if the
                                // pool is exhausted, degrade to the direct
                                // effect rather than dropping the action.
                                combat_apply_hit(e, target, gs);
                            }
                            e->attackReleaseFired = true;
                        }
                    } else if (targetAvailable) {
                        combat_apply_hit(e, target, gs);
                        e->attackReleaseFired = true;
                    }
                }
            }

            // Tick hit flash timer
            if (e->hitFlashTimer > 0.0f) {
                e->hitFlashTimer -= deltaTime;
            }

            // Clip finished — chain next swing or leave attack
            if (evt.finishedThisTick) {
                // Retarget for next swing
                entity_release_reserved_projectile(e, gs);
                entity_retarget_or_walk(e, gs);
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

    AnimPlaybackEvent evt = anim_state_update(&e->anim, deltaTime);
    entity_note_attack_progress(e, &evt);
    if (e->state == ESTATE_WALKING && evt.loopedThisTick) {
        pathfind_commit_presentation(e, &gs->battlefield);
        pathfind_update_walk_facing(e, &gs->battlefield);
    }
}

void entity_draw(const Entity *e) {
    if (!e || e->markedForRemoval || !e->sprite) return;
    sprite_draw(e->sprite, &e->anim, e->position, e->spriteScale, e->spriteRotationDegrees);
}
