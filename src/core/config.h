//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_CONFIG_H

// Screen
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

// Paths
#define CARD_SHEET_PATH     "src/assets/cards/ModularCardsRPG/modularCardsRPGSheet.png"
#define GRASS_TILESET_PATH  "src/assets/environment/Pixel Art Top Down - Basic v1.2.3/Texture/TX Tileset Grass.png"
#define UNDEAD_TILESET_PATH "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/Ground_rocks.png"
#define UNDEAD_DETAIL_PATH  "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/details.png"
#define FX_SMOKE_PATH       "src/assets/fx/smoke.png"

// Character sprites
#define CHAR_BASE_PATH      "src/assets/characters/Base/"
#define CHAR_KNIGHT_PATH    "src/assets/characters/Knight/"
#define CHAR_HEALER_PATH    "src/assets/characters/Healer/"
#define CHAR_ASSASSIN_PATH  "src/assets/characters/Assassin/"
#define CHAR_BRUTE_PATH     "src/assets/characters/Brute/"
#define CHAR_FARMER_PATH     "src/assets/characters/Farmer/"
#define SPRITE_FRAME_SIZE 79

// Gameplay tuning
#define DEFAULT_TILE_SCALE 2.0f
#define DEFAULT_TILE_SIZE  32.0f
#define DEFAULT_CARD_SCALE 2.5f

// Energy bar HUD
#define ENERGY_BAR_WIDTH    200
#define ENERGY_BAR_HEIGHT    20
#define ENERGY_BAR_MARGIN    16
#define ENERGY_BAR_Y_OFFSET  40

// Base geometry
#define BASE_SPAWN_GAP 32.0f

// Lane pathfinding
#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.3f
#define LANE_OUTER_INSET_RATIO 0.25f
#define LANE_BASE_APPROACH_START 0.72f
#define LANE_BASE_APPROACH_GAP 16.0f
#define LANE_JITTER_RADIUS   10.0f
#define PI_F 3.14159265f

// Entity / slot limits (shared by types.h and battlefield.h)
#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES   64

// Canonical board dimensions (per D-01)
#define BOARD_WIDTH   1080
#define BOARD_HEIGHT  1920
#define SEAM_Y        960

// Ore resource nodes
#define ORE_GRID_CELL_SIZE_PX        64.0f
#define ORE_GRID_COLS                16
#define ORE_GRID_ROWS                15
#define ORE_EDGE_MARGIN_CELLS        1
#define ORE_LANE_CLEARANCE_CELLS     1.0f
#define ORE_BASE_CLEARANCE_CELLS     2.0f
#define ORE_SPAWN_CLEARANCE_CELLS    1.5f
#define ORE_NODE_CLEARANCE_CELLS     1.5f
#define ORE_MATCH_COUNT_PER_SIDE     8

// Farmer tuning
#define FARMER_ORE_INTERACT_RADIUS   40.0f
#define FARMER_BASE_DEPOSIT_RADIUS   60.0f
#define FARMER_DEFAULT_ORE_VALUE     1
#define FARMER_DEFAULT_ORE_DURABILITY 1

#endif //NFC_CARDGAME_CONFIG_H
