//
// Created by Nathan Davis on 2/16/26.
//

#include "combat.h"
#include "farmer.h"
#include "win_condition.h"
#include "../core/battlefield.h"
#include "../core/battlefield_math.h"
#include "../core/debug_events.h"
#include "../entities/entities.h"
#include <math.h>
#include <float.h>
#include <stdio.h>

// All positions are now canonical -- direct distance via bf_distance (per D-18).
// Cross-space mapping and per-axis distance helpers have been deleted.

bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs) {
    (void)gs;  // No longer needs GameState for coordinate mapping
    if (!a || !b) return false;

    // Both positions are canonical -- direct distance (per D-18)
    CanonicalPos posA = { a->position };
    CanonicalPos posB = { b->position };
    return bf_distance(posA, posB) <= a->attackRange;
}

Entity *combat_find_target(Entity *attacker, GameState *gs) {
    if (!attacker || !gs) return NULL;

    Battlefield *bf = &gs->battlefield;

    Entity *bestTarget = NULL;
    float bestDist = FLT_MAX;
    CanonicalPos attackerPos = { attacker->position };

    for (int i = 0; i < bf->entityCount; i++) {
        Entity *candidate = bf->entities[i];
        if (!candidate->alive || candidate->markedForRemoval) continue;
        if (candidate->ownerID == attacker->ownerID) continue; // skip friendlies

        // Direct canonical distance -- no coordinate mapping needed
        CanonicalPos candidatePos = { candidate->position };
        float d = bf_distance(attackerPos, candidatePos);

        switch (attacker->targeting) {
            case TARGET_BUILDING:
                if (candidate->type == ENTITY_BUILDING) {
                    // Prefer buildings -- pick closest building
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
                if (d < bestDist || (bestTarget == NULL)) {
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

    // TODO: base fallback needs rework when bases are in Battlefield entity registry

    return bestTarget;
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

// Handle post-kill consequences for any entity type.
// Currently handles farmer sustenance transfer; extend for future unit roles.
static void combat_on_kill(Entity *victim, GameState *gs) {
    if (victim->unitRole == UNIT_ROLE_FARMER) {
        farmer_on_death(victim, gs);
    }
}

void combat_apply_hit(Entity *attacker, Entity *target, GameState *gs) {
    if (!attacker || !target || !gs) return;
    if (!target->alive) return;

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

    // Deal damage and reset cooldown
    bool killed = entity_take_damage(target, attacker->attack);
    attacker->attackCooldown = (attacker->attackSpeed > 0.0f)
        ? 1.0f / attacker->attackSpeed
        : 1.0f;

    printf("[COMBAT] Entity %d dealt %d damage to entity %d (hp: %d/%d)\n",
           attacker->id, attacker->attack, target->id, target->hp, target->maxHP);

    if (killed) {
        combat_on_kill(target, gs);
        if (target->type == ENTITY_BUILDING) {
            win_latch_from_destroyed_base(gs, target);
        }
    }
}
