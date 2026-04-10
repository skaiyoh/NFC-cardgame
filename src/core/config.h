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
#define STATUS_BARS_PATH    "src/assets/environment/Objects/health_energy_bars.png"

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

// Sustenance resource nodes
#define SUSTENANCE_GRID_CELL_SIZE_PX        64.0f
#define SUSTENANCE_GRID_COLS                16
#define SUSTENANCE_GRID_ROWS                15
#define SUSTENANCE_EDGE_MARGIN_CELLS        1
#define SUSTENANCE_LANE_CLEARANCE_CELLS     1.0f
#define SUSTENANCE_BASE_CLEARANCE_CELLS     2.0f
#define SUSTENANCE_SPAWN_CLEARANCE_CELLS    1.5f
#define SUSTENANCE_NODE_CLEARANCE_CELLS     1.5f
#define SUSTENANCE_MATCH_COUNT_PER_SIDE     8

// Farmer tuning
#define FARMER_SUSTENANCE_INTERACT_RADIUS   40.0f
#define FARMER_BASE_DEPOSIT_RADIUS   60.0f
#define FARMER_DEFAULT_SUSTENANCE_VALUE     1
#define FARMER_DEFAULT_SUSTENANCE_DURABILITY 1

// Hand UI (outer-edge card strip per player)
#define HAND_UI_DEPTH_PX               180
#define HAND_MAX_CARDS                 8
#define HAND_CARD_WIDTH                128
#define HAND_CARD_HEIGHT               160
#define HAND_CARD_GAP                  4
#define HAND_CARD_PLACEHOLDER_PATH     "src/assets/cards/uvulite_card.png"
#define HAND_CARD_KNIGHT_SHEET_PATH    "src/assets/cards/uvulite_card_sheet.png"
#define HAND_CARD_KNIGHT_SHEET_ROWS    1
#define HAND_CARD_KNIGHT_FRAME_COUNT   6
#define HAND_CARD_KNIGHT_FRAME_TIME    0.05f

#endif //NFC_CARDGAME_CONFIG_H
