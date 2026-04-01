//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_VIEWPORT_H
#define NFC_CARDGAME_VIEWPORT_H

#include "../core/types.h"

// Forward declarations
typedef struct Battlefield Battlefield;

// Initialize split-screen viewports for both players
void viewport_init_split_screen(GameState * gs);

// Begin rendering for a player's viewport (sets scissor mode and camera)
void viewport_begin(Player * p);

// End rendering for a player's viewport
void viewport_end(void);

// Convert world coordinates to screen coordinates for a player
Vector2 viewport_world_to_screen(Player *p, Vector2 worldPos);

// Convert screen coordinates to world coordinates for a player
Vector2 viewport_screen_to_world(Player *p, Vector2 screenPos);

// Draw tilemap for a battlefield territory
void viewport_draw_battlefield_tilemap(const Battlefield *bf, BattleSide side);

// Draw the tilemap for a player (uses player's per-biome tileDefs)
// [ADAPTER] kept during transition; use viewport_draw_battlefield_tilemap instead
void viewport_draw_tilemap(Player * p);

// Draw debug info for card slots
void viewport_draw_card_slots_debug(Player * p);

// Debug: draw lane waypoint paths in screen space from Battlefield data.
// Call outside viewports.
void debug_draw_lane_paths_screen(const Battlefield *bf, BattleSide side, Camera2D cam);

#endif //NFC_CARDGAME_VIEWPORT_H
