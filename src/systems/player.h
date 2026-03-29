//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_PLAYER_H
#define NFC_CARDGAME_PLAYER_H

#include "../core/types.h"

// Initialize a player with their viewport area and starting biome
void player_init(Player *p, int id, Rectangle playArea, Rectangle screenArea,
                 float cameraRotation, BiomeType startBiome,
                 const BiomeDef *biomeDef, float tileSize, unsigned int seed);

// Update player state (energy regen, entity updates, etc.)
void player_update(Player *p, float deltaTime);

// Clean up player resources
void player_cleanup(Player * p);

// Initialize the 3 card slot world positions based on play area
void player_init_card_slots(Player * p);

// Entity management
void player_add_entity(Player * p, Entity * entity);

void player_remove_entity(Player *p, int entityID);

Entity *player_find_entity(Player *p, int entityID);

void player_update_entities(Player *p, GameState *gs, float deltaTime);

void player_draw_entities(const Player *p);

// Card slot access
CardSlot *player_get_slot(Player *p, int slotIndex);

bool player_slot_is_available(Player *p, int slotIndex);

// --- Position helpers ---

// Convert a tile grid coordinate (col, row) to world-space center of that tile
Vector2 player_tile_to_world(Player *p, int col, int row);

// Convert world position to the nearest tile coordinate
void player_world_to_tile(Player *p, Vector2 world, int *col, int *row);

// Named landmark positions (world space)
Vector2 player_center(Player * p);
Vector2 player_base_pos(Player * p); // defensive end
Vector2 player_front_pos(Player * p); // attacking end
Vector2 player_lane_pos(Player *p, int lane, float depth); // lane 0-2, depth 0.0(base)..1.0(front)

// Troop spawn position for a given card slot (world space)
Vector2 player_slot_spawn_pos(Player *p, int slotIndex);

#endif //NFC_CARDGAME_PLAYER_H