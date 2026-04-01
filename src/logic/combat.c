//
// Created by Nathan Davis on 2/16/26.
//

#include "combat.h"
#include "../entities/entities.h"
#include <math.h>
#include <float.h>
#include <stdio.h>

// Map a position from owner's coordinate space into opponent's coordinate space.
// Same formula as game_map_crossed_world_point in game.c — duplicated here to
// keep combat self-contained without exposing a static render helper.
static Vector2 map_to_opponent_space(const Player *owner, const Player *opponent, Vector2 pos) {
    float lateral = (pos.x - owner->playArea.x) / owner->playArea.width;
    float mirroredLateral = 1.0f - lateral;
    float depth = owner->playArea.y - pos.y;
    return (Vector2){
        opponent->playArea.x + mirroredLateral * opponent->playArea.width,
        opponent->playArea.y + depth
    };
}

static float dist2d(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs) {
    if (!a || !b) return false;

    Vector2 posB;
    if (a->faction == b->faction) {
        posB = b->position;
    } else {
        // Map b's position into a's coordinate space
        const Player *ownerB = &gs->players[b->ownerID];
        const Player *ownerA = &gs->players[a->ownerID];
        posB = map_to_opponent_space(ownerB, ownerA, b->position);
    }

    return dist2d(a->position, posB) <= a->attackRange;
}

Entity *combat_find_target(Entity *attacker, GameState *gs) {
    if (!attacker || !gs) return NULL;

    int enemyID = 1 - attacker->ownerID;
    Player *enemy = &gs->players[enemyID];
    const Player *ownerA = &gs->players[attacker->ownerID];

    Entity *bestTarget = NULL;
    float bestDist = FLT_MAX;

    for (int i = 0; i < enemy->entityCount; i++) {
        Entity *candidate = enemy->entities[i];
        if (!candidate->alive || candidate->markedForRemoval) continue;

        // Map candidate position into attacker's coordinate space
        Vector2 mappedPos = map_to_opponent_space(enemy, ownerA, candidate->position);
        float d = dist2d(attacker->position, mappedPos);

        switch (attacker->targeting) {
            case TARGET_BUILDING:
                if (candidate->type == ENTITY_BUILDING) {
                    // Prefer buildings — pick closest building
                    if (d < bestDist || (bestTarget && bestTarget->type != ENTITY_BUILDING)) {
                        bestDist = d;
                        bestTarget = candidate;
                    }
                    continue;
                }
                // Fall through to nearest for non-buildings as fallback
                // fallthrough
            case TARGET_NEAREST:
            case TARGET_SPECIFIC_TYPE: // No name field on Entity yet — falls back to nearest
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
