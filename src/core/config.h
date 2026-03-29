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

// Lane pathfinding
#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.3f
#define LANE_JITTER_RADIUS   10.0f
#define PI_F 3.14159265f

#endif //NFC_CARDGAME_CONFIG_H
