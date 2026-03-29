//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_TILEMAP_RENDERER_H
#define NFC_CARDGAME_TILEMAP_RENDERER_H

#include "../../lib/raylib.h"

#define TILE_GRASS_0  0
#define TILE_GRASS_1  1
#define TILE_GRASS_2  2
#define TILE_GRASS_3  3
#define TILE_GRASS_4  4
#define TILE_GRASS_5  5
#define TILE_GRASS_6  6
#define TILE_GRASS_7  7
#define TILE_GRASS_8  8
#define TILE_GRASS_9  9
#define TILE_GRASS_10 10
#define TILE_GRASS_11 11
#define TILE_GRASS_12 12
#define TILE_GRASS_13 13
#define TILE_GRASS_14 14
#define TILE_GRASS_15 15

#define TILE_FLOWER_0  16
#define TILE_FLOWER_1  17
#define TILE_FLOWER_2  18
#define TILE_FLOWER_3  19
#define TILE_FLOWER_4  20
#define TILE_FLOWER_5  21
#define TILE_FLOWER_6  22
#define TILE_FLOWER_7  23
#define TILE_FLOWER_8  24
#define TILE_FLOWER_9  25
#define TILE_FLOWER_10 26
#define TILE_FLOWER_11 27
#define TILE_FLOWER_12 28
#define TILE_FLOWER_13 29
#define TILE_FLOWER_14 30
#define TILE_FLOWER_15 31

#define TILE_COUNT 32
#define MAX_BIOME_LAYERS 8

typedef struct {
    Texture2D *texture;
    Rectangle source;
} TileDef;

typedef struct {
    int rows;
    int cols;
    int *cells;
    int *detailCells; // overlay layer (-1 = empty, >= 0 = detail tileDef index)
    int *biomeLayerCells[MAX_BIOME_LAYERS]; // per-layer cells (NULL for PAINT layers)
    float tileSize;
    float tileScale; // source tile scale (e.g. 2.0 for 16px tiles matching 32px)
    float originX;
    float originY;
} TileMap;

// Forward declaration — full definition in biome.h
typedef struct BiomeDef BiomeDef;

void tilemap_init_defs(Texture2D *tex, TileDef tileDefs[TILE_COUNT]);

TileMap tilemap_create(Rectangle area, float tileSize, unsigned int seed);

TileMap tilemap_create_biome(Rectangle area, float tileSize, unsigned int seed,
                             const BiomeDef *biome);

void tilemap_draw(TileMap *map, TileDef tileDefs[TILE_COUNT]);

void tilemap_draw_details(TileMap *map, TileDef *detailDefs);

void tilemap_draw_biome_layers(TileMap *map, const struct BiomeDef *def);

void tilemap_free(TileMap *map);

#endif //NFC_CARDGAME_TILEMAP_RENDERER_H