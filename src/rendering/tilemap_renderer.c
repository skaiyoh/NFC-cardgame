//
// Created by Nathan Davis on 2/16/26.
//

#include "tilemap_renderer.h"
#include "biome.h"
#include <stdlib.h>
#include <string.h>

void tilemap_init_defs(Texture2D *tex, TileDef tileDefs[TILE_COUNT]) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tileDefs[r * 4 + c] = (TileDef)
            {
                .
                texture = tex,
                .
                source = (Rectangle)
                {
                    c * 32, r * 32, 32, 32
                }
            };
        }
    }

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tileDefs[16 + r * 4 + c] = (TileDef)
            {
                .
                texture = tex,
                .
                source = (Rectangle)
                {
                    128 + c * 32, r * 32, 32, 32
                }
            };
        }
    }
}

// TODO: tilemap_create (non-biome version) is dead code — all call sites use tilemap_create_biome.
// TODO: Remove this function or repurpose it, to avoid confusion about which creation path is active.
TileMap tilemap_create(Rectangle area, float tileSize, unsigned int seed) {
    TileMap map;
    map.cols = (int) (area.width / tileSize) + 1;
    map.rows = (int) (area.height / tileSize) + 1;
    map.tileSize = tileSize;
    map.originX = area.x;
    map.originY = area.y;
    map.cells = malloc(map.rows * map.cols * sizeof(int));
    map.detailCells = NULL;
    map.tileScale = 1.0f;

    srand(seed);
    for (int r = 0; r < map.rows; r++) {
        for (int c = 0; c < map.cols; c++) {
            map.cells[r * map.cols + c] =
                    (rand() % 10 < 8) ? (rand() % 16) : (16 + rand() % 16);
        }
    }

    return map;
}

TileMap tilemap_create_biome(Rectangle area, float tileSize, unsigned int seed,
                             const BiomeDef *biome) {
    TileMap map;
    // Scale adjusts the rendered tile size so different native sizes look the same.
    // e.g. 16px tiles with scale 2.0 render at tileSize/2, doubling the grid density.
    float renderSize = tileSize / biome->tileScale;
    map.cols = (int) (area.width / renderSize) + 1;
    map.rows = (int) (area.height / renderSize) + 1;
    map.tileSize = renderSize;
    map.originX = area.x;
    map.originY = area.y;
    map.cells = malloc(map.rows * map.cols * sizeof(int));
    map.tileScale = biome->tileScale;
    for (int li = 0; li < MAX_BIOME_LAYERS; li++) map.biomeLayerCells[li] = NULL;

    int totalWeight = 0;
    for (int i = 0; i < biome->blockCount; i++) {
        totalWeight += biome->blockWeights[i];
    }
    if (totalWeight <= 0) totalWeight = 1;

    // TODO: srand(seed) contaminates the global PRNG state. Any rand() calls made after this
    // TODO: (e.g. by other systems) will produce values seeded from here, breaking determinism.
    // TODO: Use a per-player LCG struct instead of the global rand() to isolate tilemap generation.
    srand(seed);
    for (int r = 0; r < map.rows; r++) {
        for (int c = 0; c < map.cols; c++) {
            int roll = rand() % totalWeight;
            int blockIdx = 0;
            int cumulative = 0;
            for (int i = 0; i < biome->blockCount; i++) {
                cumulative += biome->blockWeights[i];
                if (roll < cumulative) {
                    blockIdx = i;
                    break;
                }
            }
            int tileIdx = biome->blockStart[blockIdx]
                          + (rand() % biome->blockSize[blockIdx]);
            map.cells[r * map.cols + c] = tileIdx;
        }
    }

    // Generate detail overlay if the biome has one
    if (biome->detailDefCount > 0 && biome->detailDensity > 0) {
        int cellCount = map.rows * map.cols;
        map.detailCells = malloc(cellCount * sizeof(int));
        memset(map.detailCells, -1, cellCount * sizeof(int));

        // Manual mode (detailBlockCount == 0): uniform random from all defs
        // Block mode: weighted random by block, then random tile within block
        bool manual = (biome->detailBlockCount == 0);

        int detailTotalWeight = 0;
        if (!manual) {
            for (int i = 0; i < biome->detailBlockCount; i++) {
                detailTotalWeight += biome->detailBlockWeights[i];
            }
            if (detailTotalWeight <= 0) detailTotalWeight = 1;
        }

        for (int r = 0; r < map.rows; r++) {
            for (int c = 0; c < map.cols; c++) {
                if (rand() % 100 >= biome->detailDensity) continue;

                int detailIdx;
                if (manual) {
                    detailIdx = rand() % biome->detailDefCount;
                } else {
                    int roll = rand() % detailTotalWeight;
                    int blockIdx = 0;
                    int cumul = 0;
                    for (int i = 0; i < biome->detailBlockCount; i++) {
                        cumul += biome->detailBlockWeights[i];
                        if (roll < cumul) {
                            blockIdx = i;
                            break;
                        }
                    }
                    detailIdx = biome->detailBlockStart[blockIdx]
                                + (rand() % biome->detailBlockSize[blockIdx]);
                }
                map.detailCells[r * map.cols + c] = detailIdx;
            }
        }
    } else {
        map.detailCells = NULL;
    }

    // Generate RANDOM biome layer cells
    int n = map.rows * map.cols;
    for (int li = 0; li < biome->biomeLayerCount && li < MAX_BIOME_LAYERS; li++) {
        const BiomeLayer *bl = &biome->biomeLayerDefs[li];
        if (bl->isRandom && bl->defCount > 0) {
            map.biomeLayerCells[li] = malloc((size_t) n * sizeof(int));
            for (int i = 0; i < n; i++) {
                map.biomeLayerCells[li][i] = (rand() % 100 < bl->density)
                                                 ? (rand() % bl->defCount)
                                                 : -1;
            }
        }
        // PAINT layers: cells are static const data in BiomeLayer, no allocation needed
    }

    return map;
}

void tilemap_draw(TileMap *map, TileDef tileDefs[TILE_COUNT]) {
    for (int row = 0; row < map->rows; row++) {
        for (int col = 0; col < map->cols; col++) {
            // TODO: No bounds check on the cell value before indexing tileDefs[].
            // TODO: If map->cells[...] >= TILE_COUNT (32), this is an out-of-bounds array access.
            // TODO: biome_compile_blocks can produce indices up to blockStart[n] + blockSize[n] - 1.
            // TODO: Add: assert(map->cells[idx] < TILE_COUNT) or clamp to prevent memory corruption.
            TileDef *td = &tileDefs[map->cells[row * map->cols + col]];
            float tx = map->originX + col * map->tileSize;
            float ty = map->originY + row * map->tileSize;
            DrawTexturePro(*td->texture, td->source,
                           (Rectangle)
            {
                tx, ty, map->tileSize, map->tileSize
            }
            ,
            (Vector2)
            {
                0, 0
            }
            ,
            0.0f, WHITE
            )
            ;
        }
    }
}

void tilemap_draw_details(TileMap *map, TileDef *detailDefs) {
    if (!map->detailCells || !detailDefs) return;

    // Scale source pixels to screen pixels consistently.
    // tileScale matches the base tile ratio (e.g. 2.0 for 16px tiles on a 32px grid).
    float scale = map->tileScale;

    for (int row = 0; row < map->rows; row++) {
        for (int col = 0; col < map->cols; col++) {
            int idx = map->detailCells[row * map->cols + col];
            if (idx < 0) continue;

            TileDef *td = &detailDefs[idx];
            float dw = td->source.width * scale;
            float dh = td->source.height * scale;
            // TODO: Detail tiles can be larger than tileSize (e.g. a 40×21 pixel detail on a 32px tile).
            // TODO: The centering offsets (tileSize - dw) * 0.5 can go negative, causing the detail
            // TODO: sprite to visually bleed into adjacent tiles. Clamp or cap dw/dh to tileSize.
            float tx = map->originX + col * map->tileSize + (map->tileSize - dw) * 0.5f;
            float ty = map->originY + row * map->tileSize + (map->tileSize - dh) * 0.5f;
            DrawTexturePro(*td->texture, td->source,
                           (Rectangle)
            {
                tx, ty, dw, dh
            }
            ,
            (Vector2)
            {
                0, 0
            }
            ,
            0.0f, WHITE
            )
            ;
        }
    }
}

void tilemap_draw_biome_layers(TileMap *map, const struct BiomeDef *def) {
    if (!def || def->biomeLayerCount <= 0) return;

    for (int li = 0; li < def->biomeLayerCount && li < MAX_BIOME_LAYERS; li++) {
        const BiomeLayer *bl = &def->biomeLayerDefs[li];
        if (bl->defCount <= 0) continue;

        float scale = bl->tileScale;

        if (bl->isRandom) {
            // RANDOM layer: cells allocated in tilemap_create_biome
            int *layerCells = map->biomeLayerCells[li];
            if (!layerCells) continue;
            for (int row = 0; row < map->rows; row++) {
                for (int col = 0; col < map->cols; col++) {
                    int idx = layerCells[row * map->cols + col];
                    if (idx < 0 || idx >= bl->defCount) continue;
                    const TileDef *td = &def->biomeLayerTileDefs[li][idx];
                    float dw = td->source.width * scale;
                    float dh = td->source.height * scale;
                    float tx = map->originX + col * map->tileSize + (map->tileSize - dw) * 0.5f;
                    float ty = map->originY + row * map->tileSize + (map->tileSize - dh) * 0.5f;
                    DrawTexturePro(*td->texture, td->source,
                                   (Rectangle)
                    {
                        tx, ty, dw, dh
                    }
                    ,
                    (Vector2)
                    {
                        0, 0
                    }
                    ,
                    0.0f, WHITE
                    )
                    ;
                }
            }
        } else {
            // PAINT layer: sparse {row, col, defIdx} triples in bl->cells
            if (!bl->cells) continue;
            for (int ci = 0; ci < bl->cellCount; ci++) {
                int row = bl->cells[ci][0];
                int col = bl->cells[ci][1];
                int defIdx = bl->cells[ci][2];
                if (row < 0 || row >= map->rows) continue;
                if (col < 0 || col >= map->cols) continue;
                if (defIdx < 0 || defIdx >= bl->defCount) continue;
                const TileDef *td = &def->biomeLayerTileDefs[li][defIdx];
                float dw = td->source.width * scale;
                float dh = td->source.height * scale;
                float tx = map->originX + col * map->tileSize + (map->tileSize - dw) * 0.5f;
                float ty = map->originY + row * map->tileSize + (map->tileSize - dh) * 0.5f;
                DrawTexturePro(*td->texture, td->source,
                               (Rectangle)
                {
                    tx, ty, dw, dh
                }
                ,
                (Vector2)
                {
                    0, 0
                }
                ,
                0.0f, WHITE
                )
                ;
            }
        }
    }
}

void tilemap_free(TileMap *map) {
    free(map->cells);
    map->cells = NULL;
    free(map->detailCells);
    map->detailCells = NULL;
    for (int li = 0; li < MAX_BIOME_LAYERS; li++) {
        free(map->biomeLayerCells[li]);
        map->biomeLayerCells[li] = NULL;
    }
}