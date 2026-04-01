//
// Created by Nathan Davis on 2/16/26.
//

#include "player.h"
#include "energy.h"
#include "../logic/pathfinding.h"
#include "../entities/entities.h"
#include "../core/battlefield.h"
#include <string.h>
#include <stdio.h>

static Vector2 rect_center(Rectangle r) {
    return (Vector2)
    {
        r.x + r.width / 2.0f,
                r.y + r.height / 2.0f
    };
}

void player_init(Player *p, int id, Rectangle playArea, Rectangle screenArea,
                 float cameraRotation, BiomeType startBiome,
                 const BiomeDef *biomeDef, float tileSize, unsigned int seed) {
    memset(p, 0, sizeof(Player));

    p->id = id;
    p->playArea = playArea;
    p->screenArea = screenArea;
    p->cameraRotation = cameraRotation;
    p->biome = startBiome;
    p->biomeDef = biomeDef;

    // Copy biome tile definitions into player-local arrays
    // TODO: biome_copy_tiledefs copies TileDef structs that contain Texture2D* pointers back into
    // TODO: gs->biomeDefs[]. If biomeDefs are ever freed before players, these become dangling pointers.
    // TODO: Ensure biome_free_all is always called after player_cleanup in game_cleanup.
    biome_copy_tiledefs(biomeDef, p->tileDefs);
    p->tileDefCount = biome_tile_count(biomeDef);
    biome_copy_detail_defs(biomeDef, p->detailDefs);
    p->detailDefCount = biomeDef->detailDefCount;

    // Setup camera
    p->camera = (Camera2D)
    {
        0
    };
    p->camera.target = rect_center(playArea);
    p->camera.offset = (Vector2)
    {
        screenArea.x + screenArea.width / 2.0f,
                screenArea.y + screenArea.height / 2.0f
    };
    p->camera.rotation = cameraRotation;
    p->camera.zoom = 1.0f;

    // Create tilemap with biome-aware distribution
    // TODO: tilemap_create_biome calls srand(seed) internally, contaminating the global PRNG state.
    // TODO: Any rand() calls after this will be in an unknown-seeded state. Use a per-player LCG
    // TODO: struct instead of the global rand() to keep tilemap generation isolated.
    p->tilemap = tilemap_create_biome(playArea, tileSize, seed, biomeDef);

    // Initialize card slots
    player_init_card_slots(p);

    // Pre-compute lane waypoints (must be after player_init_card_slots)
    lane_generate_waypoints(p);

    // Initialize energy
    energy_init(p, 10.0f, 1.0f);

    // No entities yet
    p->entityCount = 0;
    // TODO: p->base is always NULL — building_create_base() returns NULL and is never called here.
    // TODO: Without a base entity, the win condition system cannot check for base destruction.
    p->base = NULL;

    printf("Player %d initialized\n", id);
}

void player_init_card_slots(Player *p) {
    // Divide play area into 3 lanes (left, center, right)
    float laneWidth = p->playArea.width / 3.0f;

    // Spawn position: near the player's edge of the play area
    // For player 0: near top of their area
    // For player 1: near bottom of their area (but coords are same due to rotation)
    float spawnY = p->playArea.y + p->playArea.height * 0.8f;

    // TODO: entity->lane is never set after a troop spawns. The slot index used for spawn position
    // TODO: is not written back to Entity.lane, breaking any future lane-based targeting logic.
    // TODO: Set e->lane = i inside spawn_troop_from_card when the slot is chosen.

    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        p->slots[i].worldPos = (Vector2)
        {
            p->playArea.x + (i + 0.5f) * laneWidth,
                    spawnY
        };
        p->slots[i].activeCard = NULL;
        p->slots[i].cooldownTimer = 0.0f;
    }
}

// Updates then sweeps dead entities via swap-with-last.
// Removes from both Player (adapter) and Battlefield (authoritative) registries.
void player_update_entities(Player *p, GameState *gs, float deltaTime) {
    // Update all entities
    for (int i = 0; i < p->entityCount; i++) {
        entity_update(p->entities[i], gs, deltaTime);
    }

    // Sweep dead/removed entities (iterate backward for safe removal)
    for (int i = p->entityCount - 1; i >= 0; i--) {
        if (p->entities[i]->markedForRemoval) {
            Entity *dead = p->entities[i];
            // Remove from Battlefield authoritative registry
            bf_remove_entity(&gs->battlefield, dead->id);
            // Swap with last element in Player adapter array
            p->entities[i] = p->entities[p->entityCount - 1];
            p->entityCount--;
            entity_destroy(dead);
        }
    }
}

// TODO: player_draw_entities is never called -- entity drawing is handled by
// TODO: game_draw_canonical_entities in game.c (iterates Battlefield.entities).
// TODO: This function is dead code. Remove in Plan 05 when Player adapter fields are deleted.
void player_draw_entities(const Player *p) {
    for (int i = 0; i < p->entityCount; i++) {
        entity_draw(p->entities[i]);
    }
}

void player_update(Player *p, float deltaTime) {
    // Update energy regeneration
    energy_update(p, deltaTime);

    // Update card slot cooldowns
    // TODO: Slot cooldown timer counts down correctly, but cooldownTimer is never set to a non-zero
    // TODO: value anywhere — slots always have cooldownTimer == 0. Set it when a card is played.
    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        if (p->slots[i].cooldownTimer > 0.0f) {
            p->slots[i].cooldownTimer -= deltaTime;
            if (p->slots[i].cooldownTimer < 0.0f) {
                p->slots[i].cooldownTimer = 0.0f;
            }
        }
    }
}

void player_cleanup(Player *p) {
    tilemap_free(&p->tilemap);

    for (int i = 0; i < p->entityCount; i++) {
        entity_destroy(p->entities[i]);
        p->entities[i] = NULL;
    }
    p->entityCount = 0;

    printf("Player %d cleaned up\n", p->id);
}

void player_add_entity(Player *p, Entity *entity) {
    if (p->entityCount >= MAX_ENTITIES) {
        fprintf(stderr, "Player %d: entity limit reached\n", p->id);
        return;
    }
    p->entities[p->entityCount++] = entity;
}

// TODO: player_remove_entity destroys the entity via entity_destroy. If the same entity is also
// TODO: marked markedForRemoval = true and swept by player_update_entities, entity_destroy will be
// TODO: called twice on the same pointer — a double-free. Do not call both paths for one entity.
// NOTE: Does not remove from Battlefield -- caller must also call bf_remove_entity if needed.
void player_remove_entity(Player *p, int entityID) {
    for (int i = 0; i < p->entityCount; i++) {
        if (p->entities[i]->id == entityID) {
            Entity *removed = p->entities[i];
            p->entities[i] = p->entities[p->entityCount - 1];
            p->entityCount--;
            entity_destroy(removed);
            return;
        }
    }
}

Entity *player_find_entity(Player *p, int entityID) {
    for (int i = 0; i < p->entityCount; i++) {
        if (p->entities[i]->id == entityID) {
            return p->entities[i];
        }
    }
    return NULL;
}

CardSlot *player_get_slot(Player *p, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) {
        return NULL;
    }
    return &p->slots[slotIndex];
}

bool player_slot_is_available(Player *p, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) {
        return false;
    }
    return p->slots[slotIndex].cooldownTimer <= 0.0f;
}

// --- Position helpers ---

Vector2 player_tile_to_world(Player *p, int col, int row) {
    float ts = p->tilemap.tileSize;
    return (Vector2)
    {
        p->tilemap.originX + col * ts + ts / 2.0f,
                p->tilemap.originY + row * ts + ts / 2.0f
    };
}

void player_world_to_tile(Player *p, Vector2 world, int *col, int *row) {
    float ts = p->tilemap.tileSize;
    *col = (int) ((world.x - p->tilemap.originX) / ts);
    *row = (int) ((world.y - p->tilemap.originY) / ts);
}

Vector2 player_center(Player *p) {
    return (Vector2)
    {
        p->playArea.x + p->playArea.width / 2.0f,
                p->playArea.y + p->playArea.height / 2.0f
    };
}

Vector2 player_base_pos(Player *p) {
    // Base sits at the back of the player's area (high Y = near their edge)
    return (Vector2)
    {
        p->playArea.x + p->playArea.width / 2.0f,
                p->playArea.y + p->playArea.height * 0.9f
    };
}

Vector2 player_front_pos(Player *p) {
    // Front line: the edge closest to the opponent
    return (Vector2)
    {
        p->playArea.x + p->playArea.width / 2.0f,
                p->playArea.y + p->playArea.height * 0.1f
    };
}

// TODO: player_lane_pos is defined but never called anywhere in the main game. It will be needed
// TODO: for lane-based troop spawning once entity->lane is properly set from slot index.
Vector2 player_lane_pos(Player *p, int lane, float depth) {
    // 3 lanes (0=left, 1=center, 2=right), depth 0.0=base .. 1.0=front
    float laneWidth = p->playArea.width / 3.0f;
    float x = p->playArea.x + (lane + 0.5f) * laneWidth;
    // Lerp from base (0.9) to front (0.1)
    float t = 0.9f - depth * 0.8f;
    float y = p->playArea.y + p->playArea.height * t;
    return (Vector2)
    {
        x, y
    };
}

Vector2 player_slot_spawn_pos(Player *p, int slotIndex) {
    CardSlot *slot = player_get_slot(p, slotIndex);
    if (!slot) return player_center(p);
    return slot->worldPos;
}