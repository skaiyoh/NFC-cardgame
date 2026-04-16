//
// Created by Nathan Davis on 2/16/26.
//

#include "combat.h"
#include "base_geometry.h"
#include "farmer.h"
#include "win_condition.h"
#include "../core/battlefield.h"
#include "../core/battlefield_math.h"
#include "../core/debug_events.h"
#include "../entities/entities.h"
#include <math.h>
#include <float.h>
#include <stdio.h>

static float combat_center_distance(Vector2 a, Vector2 b) {
    CanonicalPos posA = { a };
    CanonicalPos posB = { b };
    return bf_distance(posA, posB);
}

static bool combat_uses_base_anchor(const Entity *target) {
    if (!target) return false;
    return target->type == ENTITY_BUILDING;
}

static Vector2 combat_target_anchor(const Entity *target) {
    if (!target) return (Vector2){ 0.0f, 0.0f };
    if (combat_uses_base_anchor(target)) {
        return base_interaction_anchor(target);
    }
    return target->position;
}

static bool combat_uses_direct_range(const Entity *attacker, const Entity *target) {
    (void)target;
    if (!attacker) return true;
    return attacker->engagementMode == ATTACK_ENGAGEMENT_DIRECT_RANGE;
}

float combat_target_contact_radius(const Entity *target) {
    if (!target) return 0.0f;

    if (target->type == ENTITY_BUILDING || target->navProfile == NAV_PROFILE_STATIC) {
        float navR = (target->navRadius > 0.0f) ? target->navRadius : target->bodyRadius;
        float combatRadius = navR - COMBAT_BUILDING_MELEE_INSET;
        if (combatRadius < target->bodyRadius) combatRadius = target->bodyRadius;
        return combatRadius;
    }

    return target->bodyRadius;
}

static unsigned int combat_pair_hash(int attackerId, int targetId) {
    unsigned int x = (unsigned int)attackerId * 1103515245u;
    x ^= (unsigned int)targetId * 2654435761u;
    x ^= x >> 16;
    return x;
}

static Vector2 combat_normalize_or(Vector2 v, Vector2 fallback) {
    float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= 0.0001f) return fallback;

    float invLen = 1.0f / sqrtf(lenSq);
    return (Vector2){ v.x * invLen, v.y * invLen };
}

static float combat_tangent_bucket_for_pair(int attackerId, int targetId) {
    static const float tangentBuckets[] = { -0.95f, -0.55f, -0.2f, 0.2f, 0.55f, 0.95f };
    unsigned int hash = combat_pair_hash(attackerId, targetId);
    return tangentBuckets[hash % (sizeof(tangentBuckets) / sizeof(tangentBuckets[0]))];
}

float combat_static_target_flow_angle_degrees(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return COMBAT_STATIC_TARGET_FLOW_ANGLE_MIN_DEG;

    unsigned int hash = combat_pair_hash(attacker->id, target->id);
    float t = (float)(hash & 1023u) / 1023.0f;
    return COMBAT_STATIC_TARGET_FLOW_ANGLE_MIN_DEG +
           (COMBAT_STATIC_TARGET_FLOW_ANGLE_MAX_DEG -
            COMBAT_STATIC_TARGET_FLOW_ANGLE_MIN_DEG) * t;
}

static Vector2 combat_apply_tangent_bias(Vector2 baseDir, int attackerId,
                                         int targetId, float tangentScale) {
    Vector2 tangent = { -baseDir.y, baseDir.x };
    float tangentBucket = combat_tangent_bucket_for_pair(attackerId, targetId);
    return combat_normalize_or(
        (Vector2){
            baseDir.x + tangent.x * tangentBucket * tangentScale,
            baseDir.y + tangent.y * tangentBucket * tangentScale
        },
        baseDir
    );
}

static Vector2 combat_spread_direction(const Entity *attacker, const Entity *target) {
    Vector2 targetAnchor = combat_target_anchor(target);
    Vector2 radial = {
        attacker->position.x - targetAnchor.x,
        attacker->position.y - targetAnchor.y
    };
    Vector2 ownerFallback = (attacker->ownerID == 1)
        ? (Vector2){ 0.0f, 1.0f }
        : (Vector2){ 0.0f, -1.0f };
    radial = combat_normalize_or(radial, ownerFallback);

    return combat_apply_tangent_bias(radial, attacker->id, target->id,
                                     COMBAT_PERIMETER_TANGENT_SCALE);
}

Vector2 combat_static_target_flow_direction(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return (Vector2){ 0.0f, 0.0f };

    Vector2 targetAnchor = combat_target_anchor(target);
    Vector2 inward = {
        targetAnchor.x - attacker->position.x,
        targetAnchor.y - attacker->position.y
    };
    Vector2 ownerFallback = (attacker->ownerID == 1)
        ? (Vector2){ 0.0f, -1.0f }
        : (Vector2){ 0.0f, 1.0f };
    inward = combat_normalize_or(inward, ownerFallback);

    return combat_apply_tangent_bias(inward, attacker->id, target->id,
                                     COMBAT_STATIC_TARGET_FLOW_TANGENT_SCALE);
}

float combat_melee_reach_distance(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return 0.0f;
    if (combat_uses_direct_range(attacker, target)) {
        return attacker->attackRange;
    }

    float contactDistance = combat_target_contact_radius(target) +
                            attacker->bodyRadius +
                            PATHFIND_CONTACT_GAP;
    float slack = attacker->attackRange - contactDistance;
    if (slack < 0.0f) slack = 0.0f;
    if (slack > COMBAT_MELEE_GOAL_SLACK_MAX) slack = COMBAT_MELEE_GOAL_SLACK_MAX;
    return slack;
}

float combat_static_target_attack_radius(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return 0.0f;
    if (combat_uses_direct_range(attacker, target)) {
        return attacker->attackRange;
    }

    return combat_target_contact_radius(target) +
           attacker->bodyRadius +
           PATHFIND_CONTACT_GAP +
           combat_melee_reach_distance(attacker, target);
}

float combat_static_target_occupancy_radius(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return 0.0f;

    float attackRadius = combat_static_target_attack_radius(attacker, target);
    if (combat_uses_direct_range(attacker, target)) {
        return attackRadius;
    }

    return attackRadius + attacker->bodyRadius + PATHFIND_CONTACT_GAP;
}

bool combat_engagement_goal(const Entity *attacker, const Entity *target,
                            const Battlefield *bf, Vector2 *outGoal,
                            float *outStopRadius) {
    (void)bf;
    if (!attacker || !target || !outGoal || !outStopRadius) return false;

    if (combat_uses_direct_range(attacker, target)) {
        *outGoal = combat_target_anchor(target);
        *outStopRadius = attacker->attackRange;
        return true;
    }

    if (target->type == ENTITY_BUILDING || target->navProfile == NAV_PROFILE_STATIC) {
        *outGoal = combat_target_anchor(target);
        *outStopRadius = combat_static_target_attack_radius(attacker, target);
        return true;
    }

    Vector2 spreadDir = combat_spread_direction(attacker, target);
    Vector2 targetAnchor = combat_target_anchor(target);

    float radius = combat_target_contact_radius(target) +
                   attacker->bodyRadius +
                   PATHFIND_CONTACT_GAP;
    *outGoal = (Vector2){
        targetAnchor.x + spreadDir.x * radius,
        targetAnchor.y + spreadDir.y * radius
    };
    *outStopRadius = combat_melee_reach_distance(attacker, target);
    return true;
}

bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs) {
    (void)gs;
    if (!a || !b) return false;

    if (combat_uses_direct_range(a, b)) {
        float dist = combat_center_distance(a->position, combat_target_anchor(b));
        return dist <= a->attackRange;
    }

    if (b->type != ENTITY_BUILDING && b->navProfile != NAV_PROFILE_STATIC) {
        float contactDistance = combat_target_contact_radius(b) +
                                a->bodyRadius +
                                PATHFIND_CONTACT_GAP +
                                combat_melee_reach_distance(a, b);
        float centerDist = combat_center_distance(a->position, combat_target_anchor(b));
        return centerDist <= contactDistance + 0.001f;
    }

    float centerDist = combat_center_distance(a->position, combat_target_anchor(b));
    return centerDist <= combat_static_target_attack_radius(a, b) + 0.001f;
}

static bool combat_is_friendly_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    return target->ownerID == attacker->ownerID;
}

bool combat_can_heal_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    if (attacker->healAmount <= 0) return false;
    if (target == attacker) return false;
    if (!combat_is_friendly_target(attacker, target)) return false;
    if (target->type != ENTITY_TROOP) return false;
    if (!target->alive || target->markedForRemoval) return false;
    if (target->hp >= target->maxHP) return false;
    return true;
}

// Healer priority: nearest injured friendly troop already inside attack range.
// Returns NULL if no eligible ally exists; the caller then falls back to enemy targeting.
static Entity *combat_find_heal_target(Entity *attacker, GameState *gs) {
    Battlefield *bf = &gs->battlefield;
    Entity *bestTarget = NULL;
    float bestDist = FLT_MAX;
    CanonicalPos attackerPos = { attacker->position };

    for (int i = 0; i < bf->entityCount; i++) {
        Entity *candidate = bf->entities[i];
        if (!combat_can_heal_target(attacker, candidate)) continue;

        CanonicalPos candidatePos = { candidate->position };
        float d = bf_distance(attackerPos, candidatePos);
        if (d > attacker->attackRange) continue;                // already in range only

        if (d < bestDist) {
            bestDist = d;
            bestTarget = candidate;
        }
    }
    return bestTarget;
}

// Shared enemy-scan helper. Returns the nearest valid enemy within maxRadius
// (center-to-center canonical distance), honoring the attacker's targeting
// mode (nearest-valid, with TARGET_BUILDING priority). Pass FLT_MAX for an
// unlimited search. Does NOT run the heal-first branch -- callers that need
// heal-first semantics must run combat_find_heal_target themselves.
static Entity *combat_find_enemy_within(Entity *attacker, GameState *gs, float maxRadius) {
    Battlefield *bf = &gs->battlefield;
    Entity *bestTarget = NULL;
    float bestDist = FLT_MAX;
    CanonicalPos attackerPos = { attacker->position };

    for (int i = 0; i < bf->entityCount; i++) {
        Entity *candidate = bf->entities[i];
        if (!candidate->alive || candidate->markedForRemoval) continue;
        if (candidate->ownerID == attacker->ownerID) continue; // skip friendlies

        CanonicalPos candidatePos = { candidate->position };
        float d = bf_distance(attackerPos, candidatePos);
        if (d > maxRadius) continue;

        switch (attacker->targeting) {
            case TARGET_BUILDING:
                if (candidate->type == ENTITY_BUILDING) {
                    if (d < bestDist || (bestTarget && bestTarget->type != ENTITY_BUILDING)) {
                        bestDist = d;
                        bestTarget = candidate;
                    }
                    continue;
                }
                // Fall through to nearest for non-buildings as fallback
                // fallthrough
            case TARGET_NEAREST:
            case TARGET_SPECIFIC_TYPE: // No name field on Entity yet -- falls back to nearest
                if (d < bestDist || bestTarget == NULL) {
                    if (attacker->targeting == TARGET_BUILDING && bestTarget &&
                        bestTarget->type == ENTITY_BUILDING) {
                        // Don't replace a building target with a non-building
                        continue;
                    }
                    if (d < bestDist) {
                        bestDist = d;
                        bestTarget = candidate;
                    }
                }
                break;
        }
    }
    return bestTarget;
}

Entity *combat_find_target(Entity *attacker, GameState *gs) {
    if (!attacker || !gs) return NULL;

    if (attacker->healAmount > 0) {
        Entity *ally = combat_find_heal_target(attacker, gs);
        if (ally) return ally;
        // No injured ally in range -- fall through to normal enemy targeting.
    }

    // TODO: base fallback needs rework when bases are in Battlefield entity registry
    return combat_find_enemy_within(attacker, gs, FLT_MAX);
}

Entity *combat_find_target_within_radius(Entity *attacker, GameState *gs, float maxRadius) {
    if (!attacker || !gs) return NULL;
    if (maxRadius < 0.0f) return NULL;
    return combat_find_enemy_within(attacker, gs, maxRadius);
}

bool entity_take_damage(Entity *entity, int damage) {
    if (!entity || !entity->alive) return false;

    entity->hp -= damage;
    if (entity->hp <= 0) {
        entity->hp = 0;
        entity->alive = false;
        entity_set_state(entity, ESTATE_DEAD);
        return true;
    }
    return false;
}

bool entity_apply_heal(Entity *entity, int amount) {
    if (!entity || !entity->alive || entity->markedForRemoval) return false;
    if (amount <= 0) return false;
    if (entity->hp >= entity->maxHP) return false;

    int before = entity->hp;
    entity->hp += amount;
    if (entity->hp > entity->maxHP) entity->hp = entity->maxHP;
    return entity->hp > before;
}

static float combat_entity_sprite_height(const Entity *target) {
    if (!target || !target->sprite) return 0.0f;

    const SpriteSheet *sheet = sprite_sheet_get(target->sprite, target->anim.anim);
    if (!sheet || sheet->frameHeight <= 0) return 0.0f;

    return (float)sheet->frameHeight * target->spriteScale;
}

static Vector2 combat_damage_fx_position(const Entity *target) {
    if (!target) return (Vector2){ 0.0f, 0.0f };

    Vector2 position = target->position;
    float spriteHeight = combat_entity_sprite_height(target);
    if (spriteHeight > 0.0f) {
        position.y -= spriteHeight * 0.20f;
    }
    return position;
}

static void combat_emit_damage_fx(Entity *target, GameState *gs) {
    if (!target || !gs) return;
    if (target->type == ENTITY_BUILDING) return;

    float scale = (target->spriteScale > 0.0f) ? target->spriteScale : 1.0f;
    spawn_fx_emit_blood(&gs->spawnFx, combat_damage_fx_position(target), scale);
}

// Handle post-kill consequences for any entity type.
// Currently handles farmer sustenance transfer; extend for future unit roles.
static void combat_on_kill(Entity *victim, GameState *gs) {
    if (!victim || !gs) return;

    if (victim->unitRole == UNIT_ROLE_FARMER) {
        farmer_on_death(victim, gs);
    }
}

static bool combat_is_invalid_supporter_friendly_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    if (attacker->healAmount <= 0) return false;
    if (!combat_is_friendly_target(attacker, target)) return false;
    return !combat_can_heal_target(attacker, target);
}

bool combat_build_effect_payload(const Entity *attacker, const Entity *target,
                                 CombatEffectPayload *outPayload) {
    if (!attacker || !target || !outPayload) return false;

    if (combat_can_heal_target(attacker, target)) {
        *outPayload = (CombatEffectPayload){
            .kind = PROJECTILE_EFFECT_HEAL,
            .amount = attacker->healAmount,
            .sourceEntityId = attacker->id,
            .sourceOwnerId = attacker->ownerID,
        };
        return true;
    }

    if (combat_is_invalid_supporter_friendly_target(attacker, target)) {
        return false;
    }

    *outPayload = (CombatEffectPayload){
        .kind = PROJECTILE_EFFECT_DAMAGE,
        .amount = attacker->attack,
        .sourceEntityId = attacker->id,
        .sourceOwnerId = attacker->ownerID,
    };
    return true;
}

static bool combat_payload_can_heal_target(const CombatEffectPayload *payload, const Entity *target) {
    if (!payload || !target) return false;
    if (payload->kind != PROJECTILE_EFFECT_HEAL) return false;
    if (payload->amount <= 0) return false;
    if (target->ownerID != payload->sourceOwnerId) return false;
    if (target->type != ENTITY_TROOP) return false;
    if (!target->alive || target->markedForRemoval) return false;
    if (target->hp >= target->maxHP) return false;
    return true;
}

bool combat_apply_effect_payload(const CombatEffectPayload *payload,
                                 Entity *target, GameState *gs) {
    if (!payload || !target || !gs) return false;
    if (!target->alive || target->markedForRemoval) return false;

    if (payload->kind == PROJECTILE_EFFECT_HEAL) {
        if (!combat_payload_can_heal_target(payload, target)) {
            return false;
        }

        entity_apply_heal(target, payload->amount);
        debug_event_emit_xy(target->position.x, target->position.y, DEBUG_EVT_HIT);
        printf("[COMBAT] Entity %d healed entity %d for %d (hp: %d/%d)\n",
               payload->sourceEntityId, target->id, payload->amount, target->hp, target->maxHP);
        return true;
    }

    if (payload->kind != PROJECTILE_EFFECT_DAMAGE) {
        return false;
    }
    if (target->ownerID == payload->sourceOwnerId) {
        return false;
    }

    bool killed = entity_take_damage(target, payload->amount);
    combat_emit_damage_fx(target, gs);
    debug_event_emit_xy(target->position.x, target->position.y, DEBUG_EVT_HIT);

    printf("[COMBAT] Entity %d dealt %d damage to entity %d (hp: %d/%d)\n",
           payload->sourceEntityId, payload->amount, target->id, target->hp, target->maxHP);

    if (killed) {
        combat_on_kill(target, gs);
        if (target->type == ENTITY_BUILDING) {
            win_latch_from_destroyed_base(gs, target);
        }
    }
    return true;
}

void combat_apply_hit(Entity *attacker, Entity *target, GameState *gs) {
    CombatEffectPayload payload = {0};
    if (!attacker || !target || !gs) return;
    if (!target->alive) return;
    if (!combat_build_effect_payload(attacker, target, &payload)) return;
    combat_apply_effect_payload(&payload, target, gs);
}

void combat_apply_enemy_burst(Vector2 center, float radius, int damage,
                              int sourceEntityId, int sourceOwnerId,
                              GameState *gs) {
    if (!gs) return;
    if (damage <= 0 || radius <= 0.0f) return;

    Battlefield *bf = &gs->battlefield;
    CanonicalPos burstPos = { center };

    for (int i = 0; i < bf->entityCount; i++) {
        Entity *target = bf->entities[i];
        if (!target) continue;
        if (!target->alive || target->markedForRemoval) continue;
        if (target->type == ENTITY_PROJECTILE) continue;
        if (target->ownerID == sourceOwnerId) continue;

        CanonicalPos targetPos = { target->position };
        float dist = bf_distance(burstPos, targetPos);
        if (dist > radius) continue;

        bool killed = entity_take_damage(target, damage);
        combat_emit_damage_fx(target, gs);
        debug_event_emit_xy(target->position.x, target->position.y, DEBUG_EVT_HIT);
        printf("[COMBAT] Burst from entity %d dealt %d damage to entity %d (hp: %d/%d)\n",
               sourceEntityId, damage, target->id, target->hp, target->maxHP);

        if (killed) {
            combat_on_kill(target, gs);
            if (target->type == ENTITY_BUILDING) {
                win_latch_from_destroyed_base(gs, target);
            }
        }
    }
}

void combat_apply_king_burst(Entity *base, float radius, int damage, GameState *gs) {
    if (!base || !gs) return;
    if (damage <= 0 || radius <= 0.0f) return;
    combat_apply_enemy_burst(base->position, radius, damage, base->id, base->ownerID, gs);
}

void combat_resolve(Entity *attacker, Entity *target, GameState *gs, float deltaTime) {
    CombatEffectPayload payload = {0};
    if (!attacker || !target || !gs) return;
    if (!attacker->alive || attacker->markedForRemoval) return;
    if (!target->alive || target->markedForRemoval) return;

    // Tick cooldown
    attacker->attackCooldown -= deltaTime;
    if (attacker->attackCooldown < 0.0f) attacker->attackCooldown = 0.0f;

    // Not ready to attack yet
    if (attacker->attackCooldown > 0.0f) return;

    if (combat_build_effect_payload(attacker, target, &payload) &&
        combat_apply_effect_payload(&payload, target, gs)) {
        attacker->attackCooldown = (attacker->attackSpeed > 0.0f)
            ? 1.0f / attacker->attackSpeed
            : 1.0f;
    }
}
