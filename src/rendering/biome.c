//
// Created by Nathan Davis on 2/16/26.
//

#include "biome.h"
#include "../core/config.h"
#include <stdio.h>
#include <string.h>

#define R(x, y, w, h)  (Rectangle){ (x), (y), (w), (h) }

// ---- Per-biome definitions ----

static void biome_define_grass(BiomeDef *b) {
    b->texturePath = GRASS_TILESET_PATH;
    b->blockCount = 2;

    // Block 0: plain grass (4x4 grid at origin 0,0)
    b->blocks[0] = (TileBlock)
    {
        .
        srcX = 0,
        .
        srcY = 0,
        .
        cols = 4,
        .
        rows = 4,
        .
        tileW = 32,
        .
        tileH = 32
    };
    // Block 1: flower variants (4x4 grid at origin 128,0)
    b->blocks[1] = (TileBlock)
    {
        .
        srcX = 128,
        .
        srcY = 0,
        .
        cols = 4,
        .
        rows = 4,
        .
        tileW = 32,
        .
        tileH = 32
    };

    b->blockWeights[0] = 80;
    b->blockWeights[1] = 20;

    b->tileScale = 1.0f; // 32px native, no scaling needed

    // No detail overlay for grass (flowers are already in the base blocks)
    b->detailTexturePath = NULL;
    b->detailDensity = 0;
}

static void biome_define_undead(BiomeDef *b) {
    b->texturePath = "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/Ground_rocks.png";
    b->blockCount = 1;
    b->blocks[0] = (TileBlock)
    {
        .
        srcX = 32,
        .
        srcY = 32,
        .
        cols = 1,
        .
        rows = 1,
        .
        tileW = 16,
        .
        tileH = 16
    };
    b->blockWeights[0] = 50;
    b->tileScale = 2.0f;

    b->detailTexturePath = "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/details.png";
    b->detailBlockCount = 0;
    {
        int d = 0;
        b->detailDefCount = d;
    }
    b->detailDensity = 20;

    b->biomeLayerCount = 5;
    {
        /* Layer "Layer 1"  PAINT  tex:1  tileScale:2.0 */
        BiomeLayer *l = &b->biomeLayerDefs[0];
        l->texPath = "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/details.png";
        l->tileScale = 2.0f;
        l->isRandom = false;
        l->density = 0;
        int d = 0;
        l->defSources[d++] = R(80, 64, 16, 16);
        l->defSources[d++] = R(16, 96, 32, 32);
        l->defSources[d++] = R(80, 96, 32, 32);
        l->defSources[d++] = R(112, 96, 32, 32);
        l->defSources[d++] = R(144, 64, 64, 32);
        l->defCount = d;
        static const int cells_0[][3] = {
            {0, 3, 0},
            {0, 15, 0},
            {1, 7, 3},
            {1, 15, 2},
            {2, 1, 1},
            {2, 8, 0},
            {2, 23, 0},
            {3, 0, 0},
            {3, 21, 0},
            {3, 22, 2},
            {4, 12, 0},
            {5, 11, 1},
            {6, 21, 0},
            {7, 21, 4},
            {8, 1, 0},
            {8, 2, 1},
            {8, 10, 0},
            {8, 19, 0},
            {9, 3, 0},
            {9, 11, 0},
            {9, 12, 3},
            {11, 0, 1},
            {11, 11, 2},
            {11, 19, 0},
            {12, 5, 0},
            {13, 5, 4},
            {13, 7, 0},
            {13, 16, 0},
            {13, 21, 0},
            {14, 0, 0},
            {14, 15, 1},
            {14, 22, 3},
        };
        l->cells = cells_0;
        l->cellCount = 32;
    }
    {
        /* Layer "Layer 2"  PAINT  tex:1  tileScale:2.0 */
        BiomeLayer *l = &b->biomeLayerDefs[1];
        l->texPath = "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/details.png";
        l->tileScale = 2.0f;
        l->isRandom = false;
        l->density = 0;
        int d = 0;
        l->defSources[d++] = R(24, 136, 16, 16);
        l->defSources[d++] = R(96, 144, 32, 16);
        l->defSources[d++] = R(136, 128, 24, 24);
        l->defSources[d++] = R(160, 128, 24, 24);
        l->defSources[d++] = R(48, 128, 24, 32);
        l->defSources[d++] = R(128, 152, 16, 16);
        l->defSources[d++] = R(136, 128, 16, 16);
        l->defSources[d++] = R(160, 136, 16, 16);
        l->defSources[d++] = R(104, 128, 16, 16);
        l->defCount = d;
        static const int cells_1[][3] = {
            {2, 15, 0},
            {3, 8, 8},
            {3, 15, 2},
            {4, 0, 0},
            {4, 7, 0},
            {4, 22, 0},
            {5, 22, 8},
            {6, 3, 2},
            {6, 15, 5},
            {7, 0, 3},
            {9, 8, 5},
            {10, 2, 0},
            {10, 14, 8},
            {11, 2, 8},
            {12, 18, 4},
            {13, 9, 2},
            {13, 17, 5},
            {14, 8, 0},
            {14, 21, 0},
        };
        l->cells = cells_1;
        l->cellCount = 19;
    }
    {
        /* Layer "Layer 3"  PAINT  tex:1  tileScale:2.0 */
        BiomeLayer *l = &b->biomeLayerDefs[2];
        l->texPath = "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/details.png";
        l->tileScale = 2.0f;
        l->isRandom = false;
        l->density = 0;
        int d = 0;
        l->defSources[d++] = R(48, 16, 32, 32);
        l->defSources[d++] = R(80, 32, 16, 16);
        l->defSources[d++] = R(96, 32, 16, 16);
        l->defSources[d++] = R(112, 32, 16, 16);
        l->defSources[d++] = R(144, 32, 16, 16);
        l->defSources[d++] = R(160, 32, 16, 16);
        l->defSources[d++] = R(176, 32, 16, 16);
        l->defCount = d;
        static const int cells_2[][3] = {
            {1, 3, 6},
            {1, 11, 6},
            {1, 12, 3},
            {1, 22, 0},
            {2, 2, 1},
            {2, 21, 4},
            {3, 9, 0},
            {3, 16, 2},
            {5, 3, 2},
            {5, 9, 3},
            {5, 13, 4},
            {6, 8, 4},
            {7, 1, 4},
            {7, 13, 1},
            {8, 14, 0},
            {10, 5, 4},
            {10, 7, 2},
            {10, 21, 4},
            {11, 22, 6},
            {12, 3, 0},
            {12, 4, 6},
            {12, 13, 3},
            {12, 16, 4},
            {13, 2, 1},
            {13, 4, 6},
            {13, 12, 1},
        };
        l->cells = cells_2;
        l->cellCount = 26;
    }
    {
        /* Layer "Layer phase 4"  PAINT  tex:1  tileScale:2.0 */
        BiomeLayer *l = &b->biomeLayerDefs[3];
        l->texPath = "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/details.png";
        l->tileScale = 2.0f;
        l->isRandom = false;
        l->density = 0;
        int d = 0;
        l->defSources[d++] = R(440, 72, 16, 16);
        l->defSources[d++] = R(440, 104, 16, 16);
        l->defSources[d++] = R(440, 128, 16, 16);
        l->defSources[d++] = R(464, 136, 16, 24);
        l->defSources[d++] = R(608, 96, 16, 24);
        l->defSources[d++] = R(520, 128, 16, 16);
        l->defSources[d++] = R(576, 136, 8, 8);
        l->defSources[d++] = R(504, 128, 24, 16);
        l->defSources[d++] = R(448, 144, 8, 16);
        l->defSources[d++] = R(464, 128, 24, 16);
        l->defCount = d;
        static const int cells_3[][3] = {
            {0, 8, 5},
            {0, 16, 0},
            {1, 2, 8},
            {1, 7, 1},
            {1, 20, 0},
            {2, 16, 1},
            {2, 20, 9},
            {3, 3, 0},
            {4, 3, 7},
            {4, 15, 8},
            {5, 15, 7},
            {5, 20, 6},
            {6, 7, 0},
            {6, 21, 8},
            {7, 3, 1},
            {8, 7, 8},
            {8, 16, 0},
            {8, 20, 8},
            {9, 4, 6},
            {9, 5, 5},
            {9, 17, 4},
            {9, 18, 6},
        };
        l->cells = cells_3;
        l->cellCount = 22;
    }
    {
        /* Layer "Layer 0"  PAINT  tex:0  tileScale:2.0 */
        BiomeLayer *l = &b->biomeLayerDefs[4];
        l->texPath = "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/Ground_rocks.png";
        l->tileScale = 2.0f;
        l->isRandom = false;
        l->density = 0;
        int d = 0;
        l->defSources[d++] = R(256, 576, 16, 16);
        l->defSources[d++] = R(272, 576, 16, 16);
        l->defSources[d++] = R(288, 576, 16, 16);
        l->defSources[d++] = R(288, 592, 16, 16);
        l->defSources[d++] = R(272, 592, 16, 16);
        l->defSources[d++] = R(256, 592, 16, 16);
        l->defSources[d++] = R(256, 608, 16, 16);
        l->defSources[d++] = R(272, 608, 16, 16);
        l->defSources[d++] = R(288, 608, 16, 16);
        l->defSources[d++] = R(320, 592, 16, 16);
        l->defSources[d++] = R(336, 592, 16, 16);
        l->defSources[d++] = R(336, 576, 16, 16);
        l->defSources[d++] = R(320, 576, 16, 16);
        l->defSources[d++] = R(320, 624, 16, 16);
        l->defSources[d++] = R(336, 624, 16, 16);
        l->defSources[d++] = R(368, 624, 16, 16);
        l->defSources[d++] = R(384, 624, 16, 16);
        l->defSources[d++] = R(384, 608, 16, 16);
        l->defSources[d++] = R(368, 608, 16, 16);
        l->defSources[d++] = R(368, 576, 16, 16);
        l->defSources[d++] = R(384, 576, 16, 16);
        l->defSources[d++] = R(384, 560, 16, 16);
        l->defSources[d++] = R(368, 560, 16, 16);
        l->defCount = d;
        static const int cells_4[][3] = {
            {0, 4, 5},
            {0, 5, 4},
            {0, 6, 3},
            {0, 17, 5},
            {0, 18, 4},
            {0, 19, 3},
            {1, 4, 5},
            {1, 5, 4},
            {1, 6, 3},
            {1, 17, 5},
            {1, 18, 4},
            {1, 19, 3},
            {2, 4, 5},
            {2, 5, 4},
            {2, 6, 3},
            {2, 17, 5},
            {2, 18, 4},
            {2, 19, 3},
            {3, 4, 5},
            {3, 5, 4},
            {3, 6, 3},
            {3, 17, 5},
            {3, 18, 4},
            {3, 19, 3},
            {4, 4, 5},
            {4, 5, 4},
            {4, 6, 3},
            {4, 17, 5},
            {4, 18, 4},
            {4, 19, 3},
            {5, 4, 5},
            {5, 5, 4},
            {5, 6, 3},
            {5, 17, 5},
            {5, 18, 4},
            {5, 19, 3},
            {6, 4, 5},
            {6, 5, 4},
            {6, 6, 3},
            {6, 17, 5},
            {6, 18, 4},
            {6, 19, 3},
            {7, 4, 5},
            {7, 5, 4},
            {7, 6, 3},
            {7, 17, 5},
            {7, 18, 4},
            {7, 19, 3},
            {8, 4, 6},
            {8, 5, 7},
            {8, 6, 8},
            {8, 17, 6},
            {8, 18, 7},
            {8, 19, 8},
        };
        l->cells = cells_4;
        l->cellCount = 54;
    }
}

static void biome_define_snow(BiomeDef *b) {
    // Placeholder: reuses grass tileset until snow assets exist
    b->texturePath = GRASS_TILESET_PATH;
    b->blockCount = 2;

    b->blocks[0] = (TileBlock)
    {
        .
        srcX = 0,
        .
        srcY = 0,
        .
        cols = 4,
        .
        rows = 4,
        .
        tileW = 32,
        .
        tileH = 32
    };
    b->blocks[1] = (TileBlock)
    {
        .
        srcX = 128,
        .
        srcY = 0,
        .
        cols = 4,
        .
        rows = 4,
        .
        tileW = 32,
        .
        tileH = 32
    };

    b->blockWeights[0] = 90;
    b->blockWeights[1] = 10;

    b->tileScale = 1.0f;

    b->detailTexturePath = NULL;
    b->detailDensity = 0;
}

static void biome_define_swamp(BiomeDef *b) {
    // Placeholder: reuses grass tileset with more flowers until swamp assets exist
    b->texturePath = GRASS_TILESET_PATH;
    b->blockCount = 2;

    b->blocks[0] = (TileBlock)
    {
        .
        srcX = 0,
        .
        srcY = 0,
        .
        cols = 4,
        .
        rows = 4,
        .
        tileW = 32,
        .
        tileH = 32
    };
    b->blocks[1] = (TileBlock)
    {
        .
        srcX = 128,
        .
        srcY = 0,
        .
        cols = 4,
        .
        rows = 4,
        .
        tileW = 32,
        .
        tileH = 32
    };

    b->blockWeights[0] = 60;
    b->blockWeights[1] = 40;

    b->tileScale = 1.0f;

    b->detailTexturePath = NULL;
    b->detailDensity = 0;
}

// ---- Internal: build flat TileDef array from blocks ----

// TODO: biome_compile_blocks silently truncates when a biome's blocks exceed TILE_COUNT tiles.
// TODO: No warning or assert is emitted, so extra tiles are silently dropped. Add a fprintf(stderr,...)
// TODO: when (idx >= TILE_COUNT && remaining tiles > 0) so the biome author knows data is being lost.
void biome_compile_blocks(BiomeDef *b) {
    int idx = 0;
    for (int i = 0; i < b->blockCount && idx < TILE_COUNT; i++) {
        TileBlock *blk = &b->blocks[i];
        b->blockStart[i] = idx;
        int count = 0;
        for (int r = 0; r < blk->rows && idx < TILE_COUNT; r++) {
            for (int c = 0; c < blk->cols && idx < TILE_COUNT; c++) {
                b->tileDefs[idx] = (TileDef)
                {
                    .
                    texture = &b->texture,
                    .
                    source = (Rectangle)
                    {
                        blk->srcX + c * blk->tileW,
                                blk->srcY + r * blk->tileH,
                                blk->tileW,
                                blk->tileH
                    }
                };
                idx++;
                count++;
            }
        }
        b->blockSize[i] = count;
    }
    b->tileDefCount = idx;
}

// TODO: biome_compile_detail_blocks has the same silent-truncation issue as biome_compile_blocks.
// TODO: If detailBlocks overflow MAX_DETAIL_DEFS, excess entries are dropped without any warning.
void biome_compile_detail_blocks(BiomeDef *b) {
    int idx = 0;
    for (int i = 0; i < b->detailBlockCount && idx < MAX_DETAIL_DEFS; i++) {
        TileBlock *blk = &b->detailBlocks[i];
        b->detailBlockStart[i] = idx;
        int count = 0;
        for (int r = 0; r < blk->rows && idx < MAX_DETAIL_DEFS; r++) {
            for (int c = 0; c < blk->cols && idx < MAX_DETAIL_DEFS; c++) {
                b->detailDefs[idx] = (TileDef)
                {
                    .
                    texture = &b->detailTexture,
                    .
                    source = (Rectangle)
                    {
                        blk->srcX + c * blk->tileW,
                                blk->srcY + r * blk->tileH,
                                blk->tileW,
                                blk->tileH
                    }
                };
                idx++;
                count++;
            }
        }
        b->detailBlockSize[i] = count;
    }
    b->detailDefCount = idx;
}

// ---- Public API ----

void biome_init_all(BiomeDef biomeDefs[BIOME_COUNT]) {
    memset(biomeDefs, 0, sizeof(BiomeDef) * BIOME_COUNT);

    biome_define_grass(&biomeDefs[BIOME_GRASS]);
    biome_define_undead(&biomeDefs[BIOME_UNDEAD]);
    biome_define_snow(&biomeDefs[BIOME_SNOW]);
    biome_define_swamp(&biomeDefs[BIOME_SWAMP]);

    for (int i = 0; i < BIOME_COUNT; i++) {
        BiomeDef *b = &biomeDefs[i];
        if (!b->texturePath) continue;

        // TODO: LoadTexture return value is not checked for failure. If the texture file is missing
        // TODO: or unreadable, Raylib returns a 1x1 white fallback with id==1, and b->loaded is still
        // TODO: set to true. All tiles will silently render white. Check b->texture.id > 1 (or use
        // TODO: IsTextureValid) and log an error / early-out if the load fails.
        b->texture = LoadTexture(b->texturePath);
        SetTextureFilter(b->texture, TEXTURE_FILTER_POINT);
        b->loaded = true;

        biome_compile_blocks(b);

        // Load detail overlay texture if needed by detail defs OR by any layer.
        bool layerNeedsDetail = false;
        if (b->detailTexturePath) {
            for (int li = 0; li < b->biomeLayerCount; li++) {
                const BiomeLayer *bl = &b->biomeLayerDefs[li];
                if (bl->texPath && strcmp(bl->texPath, b->detailTexturePath) == 0) {
                    layerNeedsDetail = true;
                    break;
                }
            }
        }
        if (b->detailTexturePath &&
            (b->detailBlockCount > 0 || b->detailDefCount > 0 || layerNeedsDetail)) {
            // TODO: Same unguarded LoadTexture as above — if the detail texture is missing,
            // TODO: b->detailLoaded is set true and all detail/layer tiles draw white silently.
            b->detailTexture = LoadTexture(b->detailTexturePath);
            SetTextureFilter(b->detailTexture, TEXTURE_FILTER_POINT);
            b->detailLoaded = true;

            if (b->detailBlockCount > 0) {
                biome_compile_detail_blocks(b);
            } else {
                for (int j = 0; j < b->detailDefCount; j++) {
                    b->detailDefs[j].texture = &b->detailTexture;
                }
            }

            if (b->detailDefCount > 0) {
                printf("Biome %d: loaded %d detail defs from '%s' (density %d%%)\n",
                       i, b->detailDefCount, b->detailTexturePath, b->detailDensity);
            }
        }

        // Wire biome layer texture pointers using texPath for accurate texture selection.
        for (int li = 0; li < b->biomeLayerCount; li++) {
            BiomeLayer *bl = &b->biomeLayerDefs[li];
            Texture2D *tex = &b->texture; // default: base texture

            if (bl->texPath && b->detailTexturePath
                && strcmp(bl->texPath, b->detailTexturePath) == 0) {
                // Matches detail texture
                tex = b->detailLoaded ? &b->detailTexture : &b->texture;
            } else if (bl->texPath && strcmp(bl->texPath, b->texturePath) != 0) {
                // Doesn't match base or detail — load a fresh texture
                b->layerTextures[li] = LoadTexture(bl->texPath);
                SetTextureFilter(b->layerTextures[li], TEXTURE_FILTER_POINT);
                b->layerTextureOwned[li] = true;
                tex = &b->layerTextures[li];
                printf("Biome %d layer %d: loaded extra texture '%s'\n", i, li, bl->texPath);
            }

            for (int j = 0; j < bl->defCount; j++) {
                b->biomeLayerTileDefs[li][j].texture = tex;
                b->biomeLayerTileDefs[li][j].source = bl->defSources[j];
            }
        }

        printf("Biome %d: loaded %d tile defs from '%s'\n",
               i, b->tileDefCount, b->texturePath);
    }
}

void biome_free_all(BiomeDef biomeDefs[BIOME_COUNT]) {
    for (int i = 0; i < BIOME_COUNT; i++) {
        if (biomeDefs[i].loaded) {
            UnloadTexture(biomeDefs[i].texture);
            biomeDefs[i].loaded = false;
        }
        if (biomeDefs[i].detailLoaded) {
            UnloadTexture(biomeDefs[i].detailTexture);
            biomeDefs[i].detailLoaded = false;
        }
        for (int li = 0; li < MAX_BIOME_LAYERS; li++) {
            if (biomeDefs[i].layerTextureOwned[li]) {
                UnloadTexture(biomeDefs[i].layerTextures[li]);
                biomeDefs[i].layerTextureOwned[li] = false;
            }
        }
    }
}


//  TODO: Dangling pointer risk if biomeDefs are moved or freed before players.
void biome_copy_tiledefs(const BiomeDef *biome, TileDef outDefs[TILE_COUNT]) {
    memcpy(outDefs, biome->tileDefs, sizeof(TileDef) * TILE_COUNT);
}

void biome_copy_detail_defs(const BiomeDef *biome, TileDef outDefs[MAX_DETAIL_DEFS]) {
    memcpy(outDefs, biome->detailDefs, sizeof(TileDef) * MAX_DETAIL_DEFS);
}

int biome_tile_count(const BiomeDef *biome) {
    return biome->tileDefCount;
}

void biome_fill_def(BiomeType type, BiomeDef *out) {
    switch (type) {
        case BIOME_GRASS: biome_define_grass(out);
            break;
        case BIOME_UNDEAD: biome_define_undead(out);
            break;
        case BIOME_SNOW: biome_define_snow(out);
            break;
        case BIOME_SWAMP: biome_define_swamp(out);
            break;
        default: break;
    }
}