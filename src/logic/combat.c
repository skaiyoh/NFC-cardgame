//
// Created by Nathan Davis on 2/16/26.
//

#include "combat.h"
#include "assault_slots.h"
#include "farmer.h"
#include "win_condition.h"
#include "../core/battlefield.h"
#include "../core/battlefield_math.h"
#include "../core/debug_events.h"
#include "../entities/entities.h"
#include <math.h>
#include <float.h>
#include <stdio.h>

float combat_melee_reach_distance(const Entity *attacker, const Entity *target);

static float combat_center_distance(Vector2 a, Vector2 b) {
    CanonicalPos posA = { a };
    CanonicalPos posB = { b };
    return bf_distance(posA, posB);
}

static bool combat_uses_direct_range(const Entity *attacker, const Entity *target) {
    (void)target;
    if (!attacker) return true;
    return attacker->healAmount > 0;
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

static Vector2 combat_building_radial_goal(const Entity *attacker, const Entity *target,
                                           float radius) {
    Vector2 fromTarget = {
        attacker->position.x - target->position.x,
        attacker->position.y - target->position.y
    };
    Vector2 sideFallback = (attacker && attacker->ownerID == 1)
        ? (Vector2){ 0.0f, 1.0f }
        : (Vector2){ 0.0f, -1.0f };
    Vector2 dir = combat_normalize_or(fromTarget, sideFallback);
    return (Vector2){
        target->position.x + dir.x * radius,
        target->position.y + dir.y * radius
    };
}

static float combat_static_target_reach_radius(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return 0.0f;
    return combat_target_contact_radius(target) +
           attacker->bodyRadius +
           PATHFIND_CONTACT_GAP +
           combat_melee_reach_distance(attacker, target);
}

static Vector2 combat_building_staging_goal(const Entity *attacker, const Entity *target) {
    float queueRadius = combat_target_contact_radius(target) +
                        DEFAULT_MELEE_BODY_RADIUS +
                        BASE_ASSAULT_SLOT_GAP +
                        BASE_ASSAULT_QUEUE_RADIAL_OFFSET;
    float stagingRadius = queueRadius + attacker->bodyRadius + PATHFIND_CONTACT_GAP;
    return combat_building_radial_goal(attacker, target, stagingRadius);
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

bool combat_engagement_goal(const Entity *attacker, const Entity *target,
                            const Battlefield *bf, Vector2 *outGoal,
                            float *outStopRadius) {
    (void)bf;
    if (!attacker || !target || !outGoal || !outStopRadius) return false;

    if (combat_uses_direct_range(attacker, target)) {
        *outGoal = target->position;
        *outStopRadius = attacker->attackRange;
        return true;
    }

    if (target->type == ENTITY_BUILDING || target->navProfile == NAV_PROFILE_STATIC) {
        if (attacker->reservedAssaultTargetId == target->id &&
            attacker->reservedAssaultSlotKind == ASSAULT_SLOT_PRIMARY) {
            // Primary-slot holders are admitted into the base-contact cloud:
            // once inside the authored melee shell they can overlap other
            // admitted allies instead of fighting for an exact slot position.
            *outGoal = target->position;
            *outStopRadius = combat_static_target_reach_radius(attacker, target);
            return true;
        }

        if (attacker->reservedAssaultTargetId == target->id &&
            attacker->reservedAssaultSlotKind == ASSAULT_SLOT_QUEUE &&
            attacker->reservedAssaultSlotIndex >= 0) {
            *outGoal = assault_slots_get_position(target,
                                                  attacker->reservedAssaultSlotKind,
                                                  attacker->reservedAssaultSlotIndex);
            *outStopRadius = attacker->bodyRadius;
            return true;
        }

        *outGoal = combat_building_staging_goal(attacker, target);
        *outStopRadius = attacker->bodyRadius;
        return true;
    }

    Vector2 radial = {
        attacker->position.x - target->position.x,
        attacker->position.y - target->position.y
    };
    Vector2 ownerFallback = (attacker->ownerID == 1)
        ? (Vector2){ 0.0f, 1.0f }
        : (Vector2){ 0.0f, -1.0f };
    radial = combat_normalize_or(radial, ownerFallback);

    Vector2 tangent = { -radial.y, radial.x };
    static const float tangentBuckets[] = { -0.95f, -0.55f, -0.2f, 0.2f, 0.55f, 0.95f };
    unsigned int hash = combat_pair_hash(attacker->id, target->id);
    float tangentScale = tangentBuckets[hash % (sizeof(tangentBuckets) / sizeof(tangentBuckets[0]))];
    Vector2 spreadDir = combat_normalize_or(
        (Vector2){
            radial.x + tangent.x * tangentScale * COMBAT_PERIMETER_TANGENT_SCALE,
            radial.y + tangent.y * tangentScale * COMBAT_PERIMETER_TANGENT_SCALE
        },
        radial
    );

    float radius = combat_target_contact_radius(target) +
                   attacker->bodyRadius +
                   PATHFIND_CONTACT_GAP;
    *outGoal = (Vector2){
        target->position.x + spreadDir.x * radius,
        target->position.y + spreadDir.y * radius
    };
    *outStopRadius = combat_melee_reach_distance(attacker, target);
    return true;
}

bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs) {
    if (!a || !b) return false;

    if (combat_uses_direct_range(a, b)) {
        float dist = combat_center_distance(a->position, b->position);
        return dist <= a->attackRange;
    }

    if (b->type != ENTITY_BUILDING && b->navProfile != NAV_PROFILE_STATIC) {
        float contactDistance = combat_target_contact_radius(b) +
                                a->bodyRadius +
                                PATHFIND_CONTACT_GAP +
                                combat_melee_reach_distance(a, b);
        float centerDist = combat_center_distance(a->position, b->position);
        return centerDist <= contactDistance + 0.001f;
    }

    if (a->reservedAssaultTargetId != b->id ||
        a->reservedAssaultSlotKind != ASSAULT_SLOT_PRIMARY) {
        return false;
    }

    Vector2 goal;
    float stopRadius = 0.0f;
    if (!gs || !combat_engagement_goal(a, b, &gs->battlefield, &goal, &stopRadius)) {
        return false;
    }

    float distToGoal = combat_center_distance(a->position, goal);
    return distToGoal <= stopRadius + 0.001f;
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

// Handle post-kill consequences for any entity type.
// Currently handles farmer sustenance transfer; extend for future unit roles.
static void combat_on_kill(Entity *victim, GameState *gs) {
    if (!victim || !gs) return;

    if (victim->reservedAssaultSlotKind != ASSAULT_SLOT_NONE) {
        Battlefield *bf = &gs->battlefield;
        for (int i = 0; i < bf->entityCount; i++) {
            Entity *candidate = bf->entities[i];
            if (!candidate || candidate->markedForRemoval) continue;
            if (candidate->type != ENTITY_BUILDING && candidate->navProfile != NAV_PROFILE_STATIC) continue;
            assault_slots_release_for_entity(candidate, victim->id);
        }
        victim->reservedAssaultSlotKind = ASSAULT_SLOT_NONE;
        victim->reservedAssaultSlotIndex = -1;
        victim->reservedAssaultTargetId = -1;
        victim->engagementType = COMBAT_ENGAGEMENT_NONE;
    }

    if (victim->unitRole == UNIT_ROLE_FARMER) {
        farmer_on_death(victim, gs);
    }
}

typedef enum {
    EFFECT_NONE,
    EFFECT_HEAL,
    EFFECT_DAMAGE
} EffectResult;

static bool combat_is_invalid_supporter_friendly_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    if (attacker->healAmount <= 0) return false;
    if (!combat_is_friendly_target(attacker, target)) return false;
    return !combat_can_heal_target(attacker, target);
}

// Apply one effect (heal or damage) from attacker to target. Single choke point
// shared by combat_apply_hit (clip-driven) and combat_resolve (legacy cooldown)
// so healer semantics stay uniform across the public combat API.
static EffectResult apply_effect(Entity *attacker, Entity *target, GameState *gs) {
    if (combat_can_heal_target(attacker, target)) {
        entity_apply_heal(target, attacker->healAmount);
        debug_event_emit_xy(target->position.x, target->position.y, DEBUG_EVT_HIT);
        printf("[COMBAT] Entity %d healed entity %d for %d (hp: %d/%d)\n",
               attacker->id, target->id, attacker->healAmount, target->hp, target->maxHP);
        return EFFECT_HEAL;
    }

    if (combat_is_invalid_supporter_friendly_target(attacker, target)) {
        return EFFECT_NONE;
    }

    bool killed = entity_take_damage(target, attacker->attack);
    debug_event_emit_xy(target->position.x, target->position.y, DEBUG_EVT_HIT);

    printf("[COMBAT] Entity %d dealt %d damage to entity %d (hp: %d/%d)\n",
           attacker->id, attacker->attack, target->id, target->hp, target->maxHP);

    if (killed) {
        combat_on_kill(target, gs);
        if (target->type == ENTITY_BUILDING) {
            win_latch_from_destroyed_base(gs, target);
        }
    }
    return EFFECT_DAMAGE;
}

void combat_apply_hit(Entity *attacker, Entity *target, GameState *gs) {
    if (!attacker || !target || !gs) return;
    if (!target->alive) return;

    apply_effect(attacker, target, gs);
}

void combat_resolve(Entity *attacker, Entity *target, GameState *gs, float deltaTime) {
    if (!attacker || !target || !gs) return;
    if (!attacker->alive || attacker->markedForRemoval) return;
    if (!target->alive || target->markedForRemoval) return;

    // Tick cooldown
    attacker->attackCooldown -= deltaTime;
    if (attacker->attackCooldown < 0.0f) attacker->attackCooldown = 0.0f;

    // Not ready to attack yet
    if (attacker->attackCooldown > 0.0f) return;

    EffectResult result = apply_effect(attacker, target, gs);
    if (result != EFFECT_NONE) {
        attacker->attackCooldown = (attacker->attackSpeed > 0.0f)
            ? 1.0f / attacker->attackSpeed
            : 1.0f;
    }
}
