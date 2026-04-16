//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_BIOME_H
#define NFC_CARDGAME_BIOME_H

#include "tilemap_renderer.h"
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

// Biome types
typedef enum {
    BIOME_GRASS,
    BIOME_UNDEAD,
    BIOME_SNOW,
    BIOME_SWAMP,
    BIOME_COUNT
} BiomeType;

// Describes a rectangular grid of tiles within a sprite sheet.
// e.g. srcX=128, srcY=0, cols=phase 4, rows=phase 4, tileW=32, tileH=32
// reads a 4x4 block of 32px tiles starting at pixel (128, 0).
typedef struct {
    int srcX;
    int srcY;
    int cols;
    int rows;
    int tileW;
    int tileH;
} TileBlock;

#define MAX_TILE_BLOCKS 4

#define MAX_DETAIL_DEFS 64
#define MAX_BIOME_LAYER_DEFS 64

// A single overlay layer applied on top of the base + detail ground.
// Supports two modes:
//   RANDOM (isRandom=true):  density% of cells get a randomly chosen def.
//   PAINT  (isRandom=false): specific cells from a static const array.
typedef struct {
    const char *texPath; // texture path (compared to base/detail at init; else loaded fresh)
    float tileScale;
    bool isRandom;
    int density; // RANDOM only: 0-100 percent
    Rectangle defSources[MAX_BIOME_LAYER_DEFS];
    int defCount;
    const int (*cells)[3]; // PAINT: static {row, col, defIdx} triples; NULL for RANDOM
    int cellCount;
} BiomeLayer;

// Full definition of a biome's tileset mapping and generation weights.
typedef struct BiomeDef {
    const char *texturePath;
    Texture2D texture;
    bool loaded;

    TileBlock blocks[MAX_TILE_BLOCKS];
    int blockCount;

    // Compiled flat TileDef array built from blocks
    TileDef tileDefs[TILE_COUNT];
    int tileDefCount;

    // Per-block index ranges into tileDefs[]
    int blockStart[MAX_TILE_BLOCKS];
    int blockSize[MAX_TILE_BLOCKS];

    // Distribution weights for tilemap generation (per block)
    int blockWeights[MAX_TILE_BLOCKS];

    // Scale factor applied to source tiles so different native sizes render equally.
    // e.g. 1.0 for 32px tiles, 2.0 for 16px tiles (scales up to match 32px).
    float tileScale;

    // Detail overlay (transparent tiles drawn on top of base ground)
    const char *detailTexturePath; // NULL if no detail layer
    Texture2D detailTexture;
    bool detailLoaded;

    TileBlock detailBlocks[MAX_TILE_BLOCKS];
    int detailBlockCount;

    TileDef detailDefs[MAX_DETAIL_DEFS];
    int detailDefCount;

    int detailBlockStart[MAX_TILE_BLOCKS];
    int detailBlockSize[MAX_TILE_BLOCKS];
    int detailBlockWeights[MAX_TILE_BLOCKS];

    int detailDensity; // percentage chance (0-100) a cell gets a detail

    // Biome overlay layers (applied on top of base + detail)
    BiomeLayer biomeLayerDefs[MAX_BIOME_LAYERS];
    int biomeLayerCount;
    TileDef biomeLayerTileDefs[MAX_BIOME_LAYERS][MAX_BIOME_LAYER_DEFS]; // wired at init
    Texture2D layerTextures[MAX_BIOME_LAYERS]; // extra textures owned by this biome
    bool layerTextureOwned[MAX_BIOME_LAYERS]; // true if we must unload it
} BiomeDef;

// Compile TileBlocks into the flat tileDefs[] array (called internally by biome_init_all,
// also exposed so tools can recompile after interactive edits).
void biome_compile_blocks(BiomeDef *b);

// Compile detailBlocks into the flat detailDefs[] array (grid-based mode only).
void biome_compile_detail_blocks(BiomeDef *b);

// Initialize all biome definitions, load textures, build TileDef arrays.
void biome_init_all(BiomeDef biomeDefs[BIOME_COUNT]);

// Unload all biome textures.
void biome_free_all(BiomeDef biomeDefs[BIOME_COUNT]);

// Copy a biome's TileDef array into a player-local buffer.
void biome_copy_tiledefs(const BiomeDef *biome, TileDef outDefs[TILE_COUNT]);

// Copy a biome's detail TileDef array into a player-local buffer.
void biome_copy_detail_defs(const BiomeDef *biome, TileDef outDefs[MAX_DETAIL_DEFS]);

// Populate config fields of *out from a built-in biome definition.
// Does NOT load textures — caller is responsible for that.
// Safe to call on an already-populated BiomeDef (config fields are overwritten).
void biome_fill_def(BiomeType type, BiomeDef *out);

// Get the number of valid tile definitions for a biome.
int biome_tile_count(const BiomeDef *biome);

#endif //NFC_CARDGAME_BIOME_H
