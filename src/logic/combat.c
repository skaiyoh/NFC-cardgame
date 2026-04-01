//
// Created by Nathan Davis on 2/16/26.
//

#include "combat.h"
#include "../core/battlefield_math.h"
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

    int enemyID = 1 - attacker->ownerID;
    Player *enemy = &gs->players[enemyID];

    Entity *bestTarget = NULL;
    float bestDist = FLT_MAX;
    CanonicalPos attackerPos = { attacker->position };

    for (int i = 0; i < enemy->entityCount; i++) {
        Entity *candidate = enemy->entities[i];
        if (!candidate->alive || candidate->markedForRemoval) continue;

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

    // Fall back to enemy base if no entities found
    if (!bestTarget && enemy->base && enemy->base->alive && !enemy->base->markedForRemoval) {
        bestTarget = enemy->base;
    }

    return bestTarget;
}

void entity_take_damage(Entity *entity, int damage) {
    if (!entity || !entity->alive) return;

    entity->hp -= damage;
    if (entity->hp <= 0) {
        entity->hp = 0;
        entity->alive = false;
        entity_set_state(entity, ESTATE_DEAD);
    }
}

void combat_resolve(Entity *attacker, Entity *target, float deltaTime) {
    if (!attacker || !target) return;
    if (!attacker->alive || attacker->markedForRemoval) return;
    if (!target->alive || target->markedForRemoval) return;

    // Tick cooldown
    attacker->attackCooldown -= deltaTime;
    if (attacker->attackCooldown < 0.0f) attacker->attackCooldown = 0.0f;

    // Not ready to attack yet
    if (attacker->attackCooldown > 0.0f) return;

    // Deal damage and reset cooldown
    entity_take_damage(target, attacker->attack);
    attacker->attackCooldown = (attacker->attackSpeed > 0.0f)
        ? 1.0f / attacker->attackSpeed
        : 1.0f;

    printf("[COMBAT] Entity %d dealt %d damage to entity %d (hp: %d/%d)\n",
           attacker->id, attacker->attack, target->id, target->hp, target->maxHP);
}
