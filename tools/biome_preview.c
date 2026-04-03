//
// Biome Preview Tool
// Usage: ./biome_preview <base_tileset.png> [detail.png] [layer1.png] ...
//
// Interactive tool for defining BiomeDef tile blocks, detail sprites,
// and an arbitrary stack of paint/random overlay layers.
// Left panel : tileset texture viewer with zoom/scroll and block outlines.
// Right panel: live tilemap preview with layer overlays.
// Bottom HUD : block list, layer list, keybindings, current settings.
//
// Navigation (tileset panel):
//   Scroll wheel               : Scroll vertically
//   Shift + scroll             : Scroll horizontally
//   Middle mouse drag          : Free pan
//   + / -                      : Zoom in/out
//
// Block editing (BASE mode, activeLayer = base):
//   Mouse drag                 : Define new TileBlock (snaps to grid)
//   Right-click                : Cancel drag
//   1-4 / Tab / Shift+Tab     : Select active block
//   Left/Right                 : ±5 weight on active block
//   Del                        : Remove active block
//
// Detail editing (DETAIL mode, activeLayer = base):
//   Left-click existing rect   : Select that detail def
//   Left-click empty tile      : Add one detail def (single tile)
//   Mouse drag                 : Add all tiles in the dragged region as individual defs
//   Right-click                : Remove detail def under cursor / cancel drag
//   Del                        : Remove selected detail def (or last if none)
//   D + Left/Right             : Detail density ±5
//   P                          : Enter detail paint mode (needs defs)
//
// Detail paint mode (P, activeLayer = base, MODE_DETAIL):
//   Left-drag on preview       : Paint selected def into cells
//   Right-drag on preview      : Erase cells
//   Left-click panel           : Select brush def
//   P                          : Exit detail paint mode
//
// Layer editing (activeLayer >= 0):
//   Mouse drag (paint off)     : Add TileDef to active layer
//   Right-click (paint off)    : Remove TileDef under cursor
//   Left-click (paint on)      : Select brush def from left panel
//   D + Left/Right             : Layer density ±5 (RANDOM layers)
//
// Paint mode (P, activeLayer >= 0):
//   Left-drag on preview       : Paint selected def into cells
//   Right-drag on preview      : Erase cells
//   Ctrl+Scroll on preview     : Zoom preview
//   Middle-drag on preview     : Pan preview
//
// Layer management:
//   N                          : Add new layer (PAINT, texIdx=0)
//   Ctrl+Tab / Ctrl+Shift+Tab  : Cycle active layer (base ↔ layers)
//   V                          : Toggle active layer visibility
//   P                          : Toggle paint mode (needs defs)
//   T                          : Cycle texIdx for active layer
//   Y                          : Toggle RANDOM / PAINT type
//   Ctrl+Up / Ctrl+Down        : Reorder active layer
//   Ctrl+Del                   : Delete active layer
//
// Import:
//   Ctrl+1 / Ctrl+2 / Ctrl+3 / Ctrl+4 : Load Grass / Undead / Snow / Swamp biome
//
// Global:
//   M                          : Toggle BASE / DETAIL mode (when activeLayer = base)
//   G                          : Toggle grid overlay
//   [  /  ]                    : Grid tile width -1 / +1  (hold to repeat)
//   Shift+[  /  Shift+]        : Grid tile height -1 / +1 (hold to repeat)
//   F1 / F2 / F3 / F4 / F5    : Preset size: 8 / 16 / 32 / 48 / 64
//   Up / Down                  : tileScale ±0.5 (BASE mode, no detail def selected)
//   R                          : Regenerate tilemap (new seed)
//   E                          : Export complete biome_define_NAME() to stdout (base + detail + layers)
//   Ctrl+S                     : Save session to biome_preview.save
//   Ctrl+L                     : Load session from biome_preview.save
//

#include "../src/rendering/biome.h"
#include "../src/rendering/tilemap_renderer.h"
#include "../src/core/config.h"
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// ── Layout ──────────────────────────────────────────────────────────────────
#define WIN_W      1800
#define WIN_H      1080
#define PANEL_W     560
#define PREVIEW_X   570
#define PREVIEW_Y    10
#define PREVIEW_W  1220
#define PREVIEW_H   650

// ── Block colors ─────────────────────────────────────────────────────────────
static const Color BLOCK_COLORS[MAX_TILE_BLOCKS] = {
    { 255, 80,  80,  220 },
    { 80,  230, 80,  220 },
    { 80,  130, 255, 220 },
    { 255, 210, 50,  220 },
};

// ── Layer system ──────────────────────────────────────────────────────────────
#define MAX_LAYERS     8
#define MAX_LAYER_DEFS 64

typedef enum { LAYER_RANDOM, LAYER_PAINT } LayerType;

typedef struct {
    char      name[32];
    LayerType type;
    bool      visible;
    int       texIdx;          // index into loadedTextures[]
    float     tileScale;

    // Defs defined by dragging on left panel
    TileDef   defs[MAX_LAYER_DEFS];
    int       defCount;

    // TileBlocks (for potential block-mode future use)
    TileBlock blocks[MAX_TILE_BLOCKS];
    int       blockCount;
    int       blockWeights[MAX_TILE_BLOCKS];
    int       blockStart[MAX_TILE_BLOCKS];
    int       blockSize[MAX_TILE_BLOCKS];

    // LAYER_RANDOM
    int  density;              // 0–100

    // Cell arrays (heap: rows*cols), -1 = empty
    int *randCells;
    int *paintCells;
    int  paintRows, paintCols;
} Layer;

// ── State ────────────────────────────────────────────────────────────────────
typedef enum { MODE_BASE, MODE_DETAIL } EditMode;

typedef struct {
    // Texture pool (all argv textures)
    Texture2D loadedTextures[MAX_LAYERS];
    char      texPaths[MAX_LAYERS][256];
    int       texCount;

    BiomeDef def;
    TileMap  tmap;
    bool     tmapValid;
    int      seed;

    EditMode mode;
    int      activeBlock;      // selected block index in BASE or DETAIL block mode (-1=none)
    int      activeDetailDef;  // selected manual detail def index (-1=none)

    float zoom;
    bool  showGrid;
    int   gridTileW;
    int   gridTileH;

    bool    dragging;
    Vector2 dragStart;   // snapped texture coords
    Vector2 dragCur;     // snapped texture coords (live)

    float scrollX;
    float scrollY;

    bool    panning;     // tileset panel middle mouse pan
    Vector2 panOrigin;
    float   panScrollX;
    float   panScrollY;

    // Held-key repeat timers
    int   arrowHoldDir;
    float arrowHoldTimer;
    int   udHoldDir;
    float udHoldTimer;
    int   densityHoldDir;
    float densityHoldTimer;
    int   sizeHoldKey;
    bool  sizeHoldIsH;
    float sizeHoldTimer;

    Vector2 cursorTex;
    bool dirty;

    // Layer system
    Layer layers[MAX_LAYERS];
    int   layerCount;
    int   activeLayer;         // -1 = base biome editing
    int   brushDef;            // active paint brush def index
    bool  paintMode;
    int   hoveredPaintRow, hoveredPaintCol;
    bool  hoveredPaintActive;

    // Detail paint mode
    bool  detailPaintMode;
    int  *detailPaintCells;    // heap rows*cols, -1=empty; NULL until P pressed
    int   detailPaintRows, detailPaintCols;
    int   detailBrushDef;      // index into def.detailDefs[]

    // Preview zoom/pan
    float   previewZoom;
    float   previewPanX, previewPanY;
    bool    previewPanning;
    Vector2 previewPanOrigin;
    float   previewPanStartX, previewPanStartY;

    // Save/load
    char  savePath[256];
    char  statusMsg[320];
    float statusTimer;
} State;

// ── Helpers ──────────────────────────────────────────────────────────────────

static int wrap(int v, int max) {
    if (max <= 0) return 0;
    return ((v % max) + max) % max;
}

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static Vector2 screen_to_tex(float sx, float sy, float scrollX, float scrollY, float zoom) {
    return (Vector2){ (sx + scrollX) / zoom, (sy + scrollY) / zoom };
}

static Vector2 snap_grid(Vector2 tp, int tw, int th) {
    return (Vector2){
        floorf(tp.x / tw) * tw,
        floorf(tp.y / th) * th,
    };
}

static float max_scroll(float texPx, float panelPx, float zoom) {
    float disp = texPx * zoom;
    return disp > panelPx ? disp - panelPx : 0;
}

// Check if texture is in the pool (by GPU id) to avoid double-unload.
static bool tex_is_in_pool(const State *s, Texture2D t) {
    for (int i = 0; i < s->texCount; i++)
        if (s->loadedTextures[i].id == t.id) return true;
    return false;
}

// ── Layer helpers ─────────────────────────────────────────────────────────────

static void layer_alloc_cells(Layer *layer, int rows, int cols) {
    free(layer->randCells);
    free(layer->paintCells);
    int n = rows * cols;
    layer->randCells  = malloc(n * sizeof(int));
    layer->paintCells = malloc(n * sizeof(int));
    // -1 in each int when all bytes are 0xFF (two's complement)
    memset(layer->randCells,  0xFF, (size_t)n * sizeof(int));
    memset(layer->paintCells, 0xFF, (size_t)n * sizeof(int));
    layer->paintRows = rows;
    layer->paintCols = cols;
}

static void layer_regen_random(Layer *layer, int rows, int cols, int seed) {
    if (!layer->randCells || layer->defCount == 0) return;
    srand((unsigned int)seed);
    int n = rows * cols;
    for (int i = 0; i < n; i++) {
        if (rand() % 100 >= layer->density) {
            layer->randCells[i] = -1;
        } else {
            layer->randCells[i] = rand() % layer->defCount;
        }
    }
}

static void layer_draw(const Layer *layer, const TileMap *tmap) {
    if (!layer->visible || layer->defCount == 0) return;
    const int *cells = (layer->type == LAYER_PAINT) ? layer->paintCells : layer->randCells;
    if (!cells) return;

    float scale = layer->tileScale;
    for (int row = 0; row < tmap->rows; row++) {
        for (int col = 0; col < tmap->cols; col++) {
            int idx = cells[row * tmap->cols + col];
            if (idx < 0 || idx >= layer->defCount) continue;
            const TileDef *td = &layer->defs[idx];
            if (!td->texture) continue;
            float dw = td->source.width  * scale;
            float dh = td->source.height * scale;
            float tx = tmap->originX + col * tmap->tileSize + (tmap->tileSize - dw) * 0.5f;
            float ty = tmap->originY + row * tmap->tileSize + (tmap->tileSize - dh) * 0.5f;
            DrawTexturePro(*td->texture, td->source,
                (Rectangle){ tx, ty, dw, dh },
                (Vector2){ 0, 0 }, 0.0f, WHITE);
        }
    }
}

static void layer_draw_panel_defs(const State *s, const Layer *layer) {
    for (int j = 0; j < layer->defCount; j++) {
        Rectangle r = layer->defs[j].source;
        float dx = r.x * s->zoom - s->scrollX;
        float dy = r.y * s->zoom - s->scrollY;
        float dw = r.width  * s->zoom;
        float dh = r.height * s->zoom;
        bool isBrush = (j == s->brushDef);
        Color c = isBrush ? YELLOW : (Color){ 220, 100, 255, 200 };
        DrawRectangle((int)dx, (int)dy, (int)dw, (int)dh,
                      (Color){ c.r, c.g, c.b, isBrush ? 60 : 25 });
        DrawRectangleLines((int)dx, (int)dy, (int)dw, (int)dh, c);
        DrawText(TextFormat("%d", j), (int)dx + 2, (int)dy + 2, 10, c);
    }
}

// ── Compile ──────────────────────────────────────────────────────────────────

static void recompile(State *s) {
    biome_compile_blocks(&s->def);
    if (s->def.detailLoaded) {
        if (s->def.detailBlockCount > 0) {
            biome_compile_detail_blocks(&s->def);
        } else {
            for (int j = 0; j < s->def.detailDefCount; j++)
                s->def.detailDefs[j].texture = &s->def.detailTexture;
        }
    }
}

// ── Import ───────────────────────────────────────────────────────────────────

static void import_biome(State *s, BiomeType type) {
    const char *oldBase   = s->def.texturePath;
    const char *oldDetail = s->def.detailTexturePath;

    biome_fill_def(type, &s->def);

    // Reload base texture if path changed; skip if it's already in the pool.
    if (!s->def.loaded || oldBase != s->def.texturePath) {
        if (s->def.loaded && !tex_is_in_pool(s, s->def.texture))
            UnloadTexture(s->def.texture);
        // Check pool first
        bool found = false;
        for (int i = 0; i < s->texCount; i++) {
            if (strcmp(s->texPaths[i], s->def.texturePath) == 0) {
                s->def.texture = s->loadedTextures[i];
                found = true;
                break;
            }
        }
        if (!found) {
            s->def.texture = LoadTexture(s->def.texturePath);
            SetTextureFilter(s->def.texture, TEXTURE_FILTER_POINT);
        }
        s->def.loaded = true;
    }

    // Reload / unload detail texture
    if (s->def.detailTexturePath) {
        if (!s->def.detailLoaded || oldDetail != s->def.detailTexturePath) {
            if (s->def.detailLoaded && !tex_is_in_pool(s, s->def.detailTexture))
                UnloadTexture(s->def.detailTexture);
            bool found = false;
            for (int i = 0; i < s->texCount; i++) {
                if (strcmp(s->texPaths[i], s->def.detailTexturePath) == 0) {
                    s->def.detailTexture = s->loadedTextures[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                s->def.detailTexture = LoadTexture(s->def.detailTexturePath);
                SetTextureFilter(s->def.detailTexture, TEXTURE_FILTER_POINT);
            }
            s->def.detailLoaded = true;
        }
    } else if (s->def.detailLoaded) {
        if (!tex_is_in_pool(s, s->def.detailTexture))
            UnloadTexture(s->def.detailTexture);
        s->def.detailLoaded  = false;
        s->def.detailTexture = (Texture2D){0};
    }

    // Reset editor state
    s->activeBlock     = (s->def.blockCount > 0) ? 0 : -1;
    s->activeDetailDef = -1;
    s->dragging        = false;
    s->scrollX         = 0;
    s->scrollY         = 0;
    s->mode            = MODE_BASE;

    recompile(s);
    s->dirty = true;

    printf("Imported biome %d: %s\n", (int)type,
           s->def.texturePath ? s->def.texturePath : "(none)");
}

// ── Tilemap ──────────────────────────────────────────────────────────────────

// Resize a paint cell array, preserving data in the overlapping region.
static void paint_cells_resize(int **cells, int *oldRows, int *oldCols,
                               int newRows, int newCols) {
    if (*oldRows == newRows && *oldCols == newCols) return;
    int n = newRows * newCols;
    int *next = malloc((size_t)n * sizeof(int));
    memset(next, 0xFF, (size_t)n * sizeof(int));
    if (*cells && *oldRows > 0 && *oldCols > 0) {
        int cr = (*oldRows < newRows) ? *oldRows : newRows;
        int cc = (*oldCols < newCols) ? *oldCols : newCols;
        for (int r = 0; r < cr; r++)
            for (int c = 0; c < cc; c++)
                next[r * newCols + c] = (*cells)[r * (*oldCols) + c];
    }
    free(*cells);
    *cells   = next;
    *oldRows = newRows;
    *oldCols = newCols;
}

static void regen_tilemap(State *s) {
    if (!s->def.loaded || s->def.tileDefCount == 0) return;
    if (s->tmapValid) { tilemap_free(&s->tmap); s->tmapValid = false; }

    // Match game play area: SCREEN_HEIGHT wide × (SCREEN_WIDTH/2) tall
    Rectangle area = { PREVIEW_X, PREVIEW_Y, SCREEN_HEIGHT, SCREEN_WIDTH / 2.0f };
    s->tmap = tilemap_create_biome(area, DEFAULT_TILE_SIZE * DEFAULT_TILE_SCALE,
                                   (unsigned int)s->seed, &s->def);
    s->tmapValid = true;

    // Fit the full game area in the preview panel
    float worldW = s->tmap.cols * s->tmap.tileSize;
    float worldH = s->tmap.rows * s->tmap.tileSize;
    s->previewZoom = fminf((float)PREVIEW_W / worldW, (float)PREVIEW_H / worldH);
    s->previewPanX = 0;
    s->previewPanY = 0;

    // Update layers
    for (int i = 0; i < s->layerCount; i++) {
        Layer *layer = &s->layers[i];
        if (layer->type == LAYER_RANDOM) {
            layer_alloc_cells(layer, s->tmap.rows, s->tmap.cols);
            layer_regen_random(layer, s->tmap.rows, s->tmap.cols, s->seed + i * 31337);
        } else {
            // PAINT: resize preserving existing painted cells
            paint_cells_resize(&layer->paintCells, &layer->paintRows, &layer->paintCols,
                               s->tmap.rows, s->tmap.cols);
            // randCells for PAINT layers aren't used for drawing but keep in sync
            free(layer->randCells);
            layer->randCells = malloc((size_t)s->tmap.rows * s->tmap.cols * sizeof(int));
            memset(layer->randCells, 0xFF,
                   (size_t)s->tmap.rows * s->tmap.cols * sizeof(int));
        }
    }

    // Detail paint cells: resize preserving painted data
    if (s->detailPaintCells) {
        paint_cells_resize(&s->detailPaintCells, &s->detailPaintRows, &s->detailPaintCols,
                           s->tmap.rows, s->tmap.cols);
    }
}

// ── Export ───────────────────────────────────────────────────────────────────

// Count non-empty layers (PAINT with ≥1 painted cell, RANDOM with ≥1 def).
static int count_export_layers(const State *s) {
    int count = 0;
    for (int li = 0; li < s->layerCount; li++) {
        const Layer *layer = &s->layers[li];
        if (layer->type == LAYER_RANDOM) {
            if (layer->defCount > 0) count++;
        } else {
            // PAINT: need at least one painted cell
            bool hasCells = false;
            if (layer->paintCells) {
                for (int i = 0; i < layer->paintRows * layer->paintCols; i++) {
                    if (layer->paintCells[i] >= 0) { hasCells = true; break; }
                }
            }
            if (hasCells || layer->defCount > 0) count++;
        }
    }
    return count;
}

// Unified export: emits a complete, paste-ready biome_define_NAME() function
// including base tileset, detail overlay, and all non-empty overlay layers.
static void export_c_code(const State *s) {
    const BiomeDef *def = &s->def;
    printf("\n/* Generated by biome_preview — paste into src/rendering/biome.c */\n");
    printf("static void biome_define_NAME(BiomeDef *b) {\n");

    // ── Base tileset ──────────────────────────────────────────────────────────
    printf("    b->texturePath = \"%s\";\n",
           def->texturePath ? def->texturePath : "PATH_HERE");
    printf("    b->blockCount = %d;\n", def->blockCount);
    for (int i = 0; i < def->blockCount; i++) {
        const TileBlock *blk = &def->blocks[i];
        printf("    b->blocks[%d] = (TileBlock){ .srcX=%d, .srcY=%d, "
               ".cols=%d, .rows=%d, .tileW=%d, .tileH=%d };\n",
               i, blk->srcX, blk->srcY, blk->cols, blk->rows, blk->tileW, blk->tileH);
    }
    for (int i = 0; i < def->blockCount; i++)
        printf("    b->blockWeights[%d] = %d;\n", i, def->blockWeights[i]);
    printf("    b->tileScale = %.1ff;\n\n", def->tileScale);

    // ── Detail overlay ────────────────────────────────────────────────────────
    if (def->detailTexturePath) {
        printf("    b->detailTexturePath = \"%s\";\n", def->detailTexturePath);
        printf("    b->detailBlockCount = %d;\n", def->detailBlockCount);
        if (def->detailBlockCount > 0) {
            for (int i = 0; i < def->detailBlockCount; i++) {
                const TileBlock *blk = &def->detailBlocks[i];
                printf("    b->detailBlocks[%d] = (TileBlock){ .srcX=%d, .srcY=%d, "
                       ".cols=%d, .rows=%d, .tileW=%d, .tileH=%d };\n",
                       i, blk->srcX, blk->srcY, blk->cols, blk->rows, blk->tileW, blk->tileH);
            }
            for (int i = 0; i < def->detailBlockCount; i++)
                printf("    b->detailBlockWeights[%d] = %d;\n", i, def->detailBlockWeights[i]);
        } else {
            printf("    { int d = 0;\n");
            printf("#define R(x,y,w,h) (Rectangle){(x),(y),(w),(h)}\n");
            for (int j = 0; j < def->detailDefCount; j++) {
                Rectangle r = def->detailDefs[j].source;
                printf("      b->detailDefs[d++] = (TileDef){ .source = "
                       "R(%.0f,%.0f,%.0f,%.0f) };\n",
                       r.x, r.y, r.width, r.height);
            }
            printf("      b->detailDefCount = d;\n");
            printf("#undef R\n");
            printf("    }\n");
        }
        printf("    b->detailDensity = %d;\n", def->detailDensity);
    } else {
        printf("    b->detailTexturePath = NULL;\n");
        printf("    b->detailDensity = 0;\n");
    }

    // ── Overlay layers ────────────────────────────────────────────────────────
    int exportCount = count_export_layers(s);
    if (exportCount > 0) {
        printf("\n    b->biomeLayerCount = %d;\n", exportCount);

        int outIdx = 0;
        for (int li = 0; li < s->layerCount; li++) {
            const Layer *layer = &s->layers[li];

            // Determine if this layer is exportable
            bool hasCells = false;
            if (layer->type == LAYER_PAINT && layer->paintCells) {
                for (int i = 0; i < layer->paintRows * layer->paintCols; i++) {
                    if (layer->paintCells[i] >= 0) { hasCells = true; break; }
                }
            }
            bool isExportable = (layer->type == LAYER_RANDOM && layer->defCount > 0)
                             || (layer->type == LAYER_PAINT && (hasCells || layer->defCount > 0));
            if (!isExportable) continue;

            // Resolve actual texture path from the tool's texture pool.
            const char *layerTexPath = NULL;
            if (layer->texIdx >= 0 && layer->texIdx < s->texCount) {
                layerTexPath = s->texPaths[layer->texIdx];
            } else if (layer->texIdx == 0 && s->def.texturePath) {
                layerTexPath = s->def.texturePath;
            } else if (layer->texIdx == 1 && s->def.detailTexturePath) {
                layerTexPath = s->def.detailTexturePath;
            }

            const char *typeName = (layer->type == LAYER_PAINT) ? "PAINT" : "RANDOM";
            printf("    { /* Layer \"%s\"  %s  tex:%d  tileScale:%.1f */\n",
                   layer->name, typeName, layer->texIdx, layer->tileScale);
            printf("        BiomeLayer *l = &b->biomeLayerDefs[%d];\n", outIdx);
            printf("        l->texPath = \"%s\"; l->tileScale = %.1ff;\n",
                   layerTexPath ? layerTexPath : "PATH_HERE", layer->tileScale);
            printf("        l->isRandom = %s; l->density = %d;\n",
                   (layer->type == LAYER_RANDOM) ? "true" : "false",
                   (layer->type == LAYER_RANDOM) ? layer->density : 0);

            // Def source rects
            if (layer->defCount > 0) {
                printf("        int d = 0;\n");
                printf("#define R(x,y,w,h) (Rectangle){(x),(y),(w),(h)}\n");
                for (int j = 0; j < layer->defCount; j++) {
                    Rectangle r = layer->defs[j].source;
                    printf("        l->defSources[d++] = R(%.0f,%.0f,%.0f,%.0f);\n",
                           r.x, r.y, r.width, r.height);
                }
                printf("        l->defCount = d;\n");
                printf("#undef R\n");
            } else {
                printf("        l->defCount = 0;\n");
            }

            if (layer->type == LAYER_PAINT) {
                // Count painted cells
                int cellCount = 0;
                if (layer->paintCells) {
                    for (int i = 0; i < layer->paintRows * layer->paintCols; i++) {
                        if (layer->paintCells[i] >= 0) cellCount++;
                    }
                }
                if (cellCount > 0) {
                    printf("        static const int cells_%d[][3] = {\n", outIdx);
                    for (int r = 0; r < layer->paintRows; r++) {
                        for (int c = 0; c < layer->paintCols; c++) {
                            int idx = layer->paintCells[r * layer->paintCols + c];
                            if (idx >= 0)
                                printf("            {%d,%d,%d},\n", r, c, idx);
                        }
                    }
                    printf("        };\n");
                    printf("        l->cells = cells_%d;\n", outIdx);
                    printf("        l->cellCount = %d;\n", cellCount);
                } else {
                    printf("        l->cells = NULL; l->cellCount = 0;\n");
                }
            } else {
                printf("        l->cells = NULL; l->cellCount = 0;\n");
            }

            printf("    }\n");
            outIdx++;
        }
    }

    printf("}\n\n");
    fflush(stdout);
}

// ── Save / Load ──────────────────────────────────────────────────────────────

#define SAVE_MAGIC   0xB104DEF1u
#define SAVE_VERSION 1u

typedef struct {
    uint32_t  magic, version;
    // Editor state
    int       mode, activeBlock, activeDetailDef, activeLayer;
    float     zoom, scrollX, scrollY;
    int       gridTileW, gridTileH, showGrid, seed;
    float     previewZoom, previewPanX, previewPanY;
    // BiomeDef config
    int       blockCount;
    TileBlock blocks[MAX_TILE_BLOCKS];
    int       blockWeights[MAX_TILE_BLOCKS];
    float     tileScale;
    int       detailBlockCount;
    TileBlock detailBlocks[MAX_TILE_BLOCKS];
    int       detailBlockWeights[MAX_TILE_BLOCKS];
    int       detailDefCount;
    int       detailDensity;
    // Detail paint
    int       detailPaintRows, detailPaintCols;
    // Layers
    int       layerCount;
} SaveHeader;

typedef struct {
    char  name[32];
    int   type, visible, texIdx;
    float tileScale;
    int   density, defCount, paintRows, paintCols;
} SaveLayer;

static void save_state(State *s, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(s->statusMsg, sizeof s->statusMsg, "Save failed: %s", path);
        s->statusTimer = 3.0f;
        return;
    }

    SaveHeader h = {0};
    h.magic              = SAVE_MAGIC;
    h.version            = SAVE_VERSION;
    h.mode               = (int)s->mode;
    h.activeBlock        = s->activeBlock;
    h.activeDetailDef    = s->activeDetailDef;
    h.activeLayer        = s->activeLayer;
    h.zoom               = s->zoom;
    h.scrollX            = s->scrollX;
    h.scrollY            = s->scrollY;
    h.gridTileW          = s->gridTileW;
    h.gridTileH          = s->gridTileH;
    h.showGrid           = s->showGrid ? 1 : 0;
    h.seed               = s->seed;
    h.previewZoom        = s->previewZoom;
    h.previewPanX        = s->previewPanX;
    h.previewPanY        = s->previewPanY;
    h.blockCount         = s->def.blockCount;
    memcpy(h.blocks,             s->def.blocks,             sizeof h.blocks);
    memcpy(h.blockWeights,       s->def.blockWeights,       sizeof h.blockWeights);
    h.tileScale          = s->def.tileScale;
    h.detailBlockCount   = s->def.detailBlockCount;
    memcpy(h.detailBlocks,       s->def.detailBlocks,       sizeof h.detailBlocks);
    memcpy(h.detailBlockWeights, s->def.detailBlockWeights, sizeof h.detailBlockWeights);
    h.detailDefCount     = s->def.detailDefCount;
    h.detailDensity      = s->def.detailDensity;
    h.detailPaintRows    = s->detailPaintCells ? s->detailPaintRows : 0;
    h.detailPaintCols    = s->detailPaintCells ? s->detailPaintCols : 0;
    h.layerCount         = s->layerCount;
    fwrite(&h, sizeof h, 1, f);

    // Detail def source rects
    for (int j = 0; j < s->def.detailDefCount; j++)
        fwrite(&s->def.detailDefs[j].source, sizeof(Rectangle), 1, f);

    // Detail paint cells
    if (h.detailPaintRows > 0) {
        int n = h.detailPaintRows * h.detailPaintCols;
        fwrite(s->detailPaintCells, sizeof(int), (size_t)n, f);
    }

    // Layers
    for (int i = 0; i < s->layerCount; i++) {
        const Layer *layer = &s->layers[i];
        SaveLayer sl = {0};
        memcpy(sl.name, layer->name, 32);
        sl.type      = (int)layer->type;
        sl.visible   = layer->visible ? 1 : 0;
        sl.texIdx    = layer->texIdx;
        sl.tileScale = layer->tileScale;
        sl.density   = layer->density;
        sl.defCount  = layer->defCount;
        sl.paintRows = layer->paintCells ? layer->paintRows : 0;
        sl.paintCols = layer->paintCells ? layer->paintCols : 0;
        fwrite(&sl, sizeof sl, 1, f);

        for (int j = 0; j < layer->defCount; j++)
            fwrite(&layer->defs[j].source, sizeof(Rectangle), 1, f);

        if (sl.paintRows > 0) {
            int n = sl.paintRows * sl.paintCols;
            fwrite(layer->paintCells, sizeof(int), (size_t)n, f);
        }
    }

    fclose(f);
    snprintf(s->statusMsg, sizeof s->statusMsg, "Saved → %s", path);
    s->statusTimer = 3.0f;
    printf("Saved to '%s'\n", path);
    fflush(stdout);
}

static void load_state(State *s, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(s->statusMsg, sizeof s->statusMsg, "Load failed: %s", path);
        s->statusTimer = 3.0f;
        return;
    }

    SaveHeader h;
    if (fread(&h, sizeof h, 1, f) != 1 ||
        h.magic != SAVE_MAGIC || h.version != SAVE_VERSION) {
        snprintf(s->statusMsg, sizeof s->statusMsg, "Bad save file: %s", path);
        s->statusTimer = 3.0f;
        fclose(f);
        return;
    }

    // Free existing dynamic data
    for (int i = 0; i < s->layerCount; i++) {
        free(s->layers[i].randCells);
        free(s->layers[i].paintCells);
        s->layers[i].randCells  = NULL;
        s->layers[i].paintCells = NULL;
    }
    free(s->detailPaintCells);
    s->detailPaintCells = NULL;

    // Restore editor state
    s->mode            = (EditMode)h.mode;
    s->activeBlock     = h.activeBlock;
    s->activeDetailDef = h.activeDetailDef;
    s->activeLayer     = h.activeLayer;
    s->zoom            = h.zoom;
    s->scrollX         = h.scrollX;
    s->scrollY         = h.scrollY;
    s->gridTileW       = h.gridTileW;
    s->gridTileH       = h.gridTileH;
    s->showGrid        = h.showGrid != 0;
    s->seed            = h.seed;
    s->previewZoom     = h.previewZoom;
    s->previewPanX     = h.previewPanX;
    s->previewPanY     = h.previewPanY;
    s->paintMode       = false;
    s->detailPaintMode = false;
    s->dragging        = false;
    s->brushDef        = -1;

    // Restore BiomeDef config
    s->def.blockCount        = h.blockCount;
    memcpy(s->def.blocks,             h.blocks,             sizeof h.blocks);
    memcpy(s->def.blockWeights,       h.blockWeights,       sizeof h.blockWeights);
    s->def.tileScale         = h.tileScale;
    s->def.detailBlockCount  = h.detailBlockCount;
    memcpy(s->def.detailBlocks,       h.detailBlocks,       sizeof h.detailBlocks);
    memcpy(s->def.detailBlockWeights, h.detailBlockWeights, sizeof h.detailBlockWeights);
    s->def.detailDefCount    = h.detailDefCount;
    s->def.detailDensity     = h.detailDensity;

    // Detail def source rects — wire texture pointers from already-loaded textures
    for (int j = 0; j < h.detailDefCount; j++) {
        fread(&s->def.detailDefs[j].source, sizeof(Rectangle), 1, f);
        s->def.detailDefs[j].texture = s->def.detailLoaded ? &s->def.detailTexture : NULL;
    }

    // Detail paint cells
    if (h.detailPaintRows > 0 && h.detailPaintCols > 0) {
        int n = h.detailPaintRows * h.detailPaintCols;
        s->detailPaintCells = malloc((size_t)n * sizeof(int));
        fread(s->detailPaintCells, sizeof(int), (size_t)n, f);
        s->detailPaintRows = h.detailPaintRows;
        s->detailPaintCols = h.detailPaintCols;
    }

    // Layers
    s->layerCount = clampi(h.layerCount, 0, MAX_LAYERS);
    for (int i = 0; i < s->layerCount; i++) {
        Layer *layer = &s->layers[i];
        memset(layer, 0, sizeof(Layer));

        SaveLayer sl;
        fread(&sl, sizeof sl, 1, f);
        memcpy(layer->name, sl.name, 32);
        layer->type      = (LayerType)sl.type;
        layer->visible   = sl.visible != 0;
        layer->texIdx    = sl.texIdx;
        layer->tileScale = sl.tileScale;
        layer->density   = sl.density;
        layer->defCount  = clampi(sl.defCount, 0, MAX_LAYER_DEFS);

        Texture2D *tex = (sl.texIdx >= 0 && sl.texIdx < s->texCount)
                         ? &s->loadedTextures[sl.texIdx] : NULL;
        for (int j = 0; j < layer->defCount; j++) {
            fread(&layer->defs[j].source, sizeof(Rectangle), 1, f);
            layer->defs[j].texture = tex;
        }

        if (sl.paintRows > 0 && sl.paintCols > 0) {
            int n = sl.paintRows * sl.paintCols;
            layer->paintCells = malloc((size_t)n * sizeof(int));
            fread(layer->paintCells, sizeof(int), (size_t)n, f);
            layer->paintRows = sl.paintRows;
            layer->paintCols = sl.paintCols;
        }
    }

    fclose(f);

    // Recompile BiomeDef and regenerate tilemap
    if (s->def.loaded)
        recompile(s);
    s->dirty = true;

    snprintf(s->statusMsg, sizeof s->statusMsg, "Loaded ← %s", path);
    s->statusTimer = 3.0f;
    printf("Loaded from '%s'\n", path);
    fflush(stdout);
}

// ── Panel helpers ─────────────────────────────────────────────────────────────

// Returns the texture currently shown in the tileset panel.
static Texture2D *panel_tex(State *s) {
    if (s->activeLayer >= 0 && s->activeLayer < s->layerCount) {
        int ti = s->layers[s->activeLayer].texIdx;
        if (ti >= 0 && ti < s->texCount)
            return &s->loadedTextures[ti];
    }
    return (s->mode == MODE_DETAIL && s->def.detailLoaded)
           ? &s->def.detailTexture : &s->def.texture;
}

static void draw_scrollbar_v(float scrollY, float texH, float zoom, int panelH) {
    float disp = texH * zoom;
    if (disp <= panelH) return;
    float ratio  = (float)panelH / disp;
    float barH   = ratio * (panelH - 4);
    float barY   = (scrollY / disp) * (panelH - 4);
    DrawRectangle(PANEL_W - 5, (int)barY + 2, 3, (int)barH,
                  (Color){ 160, 160, 160, 180 });
}

static void draw_scrollbar_h(float scrollX, float texW, float zoom, int panelW) {
    float disp = texW * zoom;
    if (disp <= panelW) return;
    float ratio  = (float)panelW / disp;
    float barW   = ratio * (panelW - 4);
    float barX   = (scrollX / disp) * (panelW - 4);
    DrawRectangle((int)barX + 2, WIN_H - 5, (int)barW, 3,
                  (Color){ 160, 160, 160, 180 });
}

// ── Tileset panel ─────────────────────────────────────────────────────────────

static void draw_tileset_panel(State *s, bool mouseInPanel) {
    DrawRectangle(0, 0, PANEL_W, WIN_H, (Color){ 28, 28, 28, 255 });
    DrawLine(PANEL_W, 0, PANEL_W, WIN_H, (Color){ 60, 60, 60, 255 });

    if (!s->def.loaded && s->texCount == 0) {
        DrawText("No texture loaded", 14, 20, 16, GRAY);
        DrawText("./biome_preview base.png [detail.png] [layer.png]...", 14, 46, 12, DARKGRAY);
        return;
    }

    Texture2D *tex = panel_tex(s);
    if (!tex || tex->id == 0) {
        DrawText("No texture for active layer", 14, 20, 14, ORANGE);
        return;
    }
    float texW = (float)tex->width;
    float texH = (float)tex->height;

    BeginScissorMode(0, 0, PANEL_W, WIN_H);

    DrawTextureEx(*tex, (Vector2){ -s->scrollX, -s->scrollY }, 0.0f, s->zoom, WHITE);

    // Grid overlay
    if (s->showGrid) {
        float tw    = s->gridTileW * s->zoom;
        float th    = s->gridTileH * s->zoom;
        float dispW = texW * s->zoom;
        float dispH = texH * s->zoom;
        Color gc = { 255, 255, 255, 50 };

        float startX = fmodf(-s->scrollX, tw);
        float endX   = fminf((float)PANEL_W, dispW - s->scrollX);
        for (float x = startX; x < endX; x += tw)
            DrawLineV((Vector2){ x, 0 }, (Vector2){ x, (float)WIN_H }, gc);

        float startY = fmodf(-s->scrollY, th);
        float endY   = fminf((float)WIN_H, dispH - s->scrollY);
        for (float y = startY; y < endY; y += th)
            DrawLineV((Vector2){ 0, y }, (Vector2){ (float)PANEL_W, y }, gc);
    }

    if (s->activeLayer >= 0 && s->activeLayer < s->layerCount) {
        // Layer mode: draw layer defs
        layer_draw_panel_defs(s, &s->layers[s->activeLayer]);
    } else if (s->mode == MODE_BASE) {
        // Existing base blocks
        for (int i = 0; i < s->def.blockCount; i++) {
            TileBlock *blk = &s->def.blocks[i];
            float bx = blk->srcX * s->zoom - s->scrollX;
            float by = blk->srcY * s->zoom - s->scrollY;
            float bw = blk->cols * blk->tileW * s->zoom;
            float bh = blk->rows * blk->tileH * s->zoom;
            Color c = BLOCK_COLORS[i % MAX_TILE_BLOCKS];
            DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, (Color){ c.r, c.g, c.b, 35 });
            bool active = (i == s->activeBlock);
            int thick = active ? 2 : 1;
            for (int t = 0; t < thick; t++)
                DrawRectangleLines((int)bx+t, (int)by+t, (int)bw-2*t, (int)bh-2*t, c);
            DrawText(TextFormat("%d", i), (int)bx + 4, (int)by + 4, 14,
                     active ? YELLOW : c);
        }
    } else {
        // Detail mode: draw each TileDef entry
        for (int j = 0; j < s->def.detailDefCount; j++) {
            Rectangle r = s->def.detailDefs[j].source;
            float dx = r.x * s->zoom - s->scrollX;
            float dy = r.y * s->zoom - s->scrollY;
            float dw = r.width  * s->zoom;
            float dh = r.height * s->zoom;
            bool active = s->detailPaintMode ? (j == s->detailBrushDef)
                                             : (j == s->activeDetailDef);
            Color c = active ? YELLOW : (Color){ 220, 100, 255, 200 };
            DrawRectangle((int)dx, (int)dy, (int)dw, (int)dh,
                          (Color){ c.r, c.g, c.b, active ? 50 : 25 });
            DrawRectangleLines((int)dx, (int)dy, (int)dw, (int)dh, c);
            DrawText(TextFormat("%d", j), (int)dx + 2, (int)dy + 2, 10, c);
        }
    }

    // Drag preview
    if (s->dragging && mouseInPanel) {
        float ax = fminf(s->dragStart.x, s->dragCur.x);
        float ay = fminf(s->dragStart.y, s->dragCur.y);
        float bx = fmaxf(s->dragStart.x, s->dragCur.x) + s->gridTileW;
        float by = fmaxf(s->dragStart.y, s->dragCur.y) + s->gridTileH;
        float sx = ax * s->zoom - s->scrollX;
        float sy = ay * s->zoom - s->scrollY;
        float sw = (bx - ax) * s->zoom;
        float sh = (by - ay) * s->zoom;
        DrawRectangle((int)sx, (int)sy, (int)sw, (int)sh,
                      (Color){ 255, 255, 255, 25 });
        DrawRectangleLines((int)sx, (int)sy, (int)sw, (int)sh, WHITE);
    }

    if (s->panning) {
        Vector2 mp = GetMousePosition();
        DrawCircleLines((int)mp.x, (int)mp.y, 8, GRAY);
    }

    EndScissorMode();

    draw_scrollbar_v(s->scrollY, texH, s->zoom, WIN_H);
    draw_scrollbar_h(s->scrollX, texW, s->zoom, PANEL_W);

    // Panel label
    const char *label;
    if (s->activeLayer >= 0 && s->activeLayer < s->layerCount) {
        Layer *layer = &s->layers[s->activeLayer];
        label = TextFormat("LAYER %d: %s  [tex:%d]", s->activeLayer, layer->name, layer->texIdx);
    } else {
        label = (s->mode == MODE_DETAIL)
                    ? (s->detailPaintMode ? "DETAIL PAINT" : "DETAIL TEXTURE")
                    : "BASE TILESET";
    }
    DrawText(label, 6, WIN_H - 20, 11, (Color){ 120, 120, 120, 255 });
    DrawText(TextFormat("%dx%d px", tex->width, tex->height),
             6, WIN_H - 34, 10, DARKGRAY);
}

// ── HUD ──────────────────────────────────────────────────────────────────────

static void draw_hud(State *s) {
    int hx = PREVIEW_X;
    int hy = PREVIEW_Y + PREVIEW_H + 14;
    const int FS = 13;
    const int LH = 17;

    DrawText("BIOME PREVIEW TOOL", hx, hy, 18, WHITE);
    hy += 26;

    DrawText("Scroll: vertical  |  Shift+scroll: horizontal  |  Middle drag: free pan  |  +/-: Zoom",
             hx, hy, 10, GRAY);
    hy += 13;
    DrawText("Import: Ctrl+1=Grass  Ctrl+2=Undead  Ctrl+3=Snow  Ctrl+4=Swamp",
             hx, hy, 10, (Color){ 180, 180, 100, 255 });
    hy += 13;

    if (s->activeLayer >= 0) {
        Layer *layer = &s->layers[s->activeLayer];
        if (s->paintMode) {
            DrawText("PAINT MODE: left-drag=paint  right-drag=erase  click panel=select brush  P=exit",
                     hx, hy, 10, (Color){ 100, 255, 200, 255 });
        } else {
            DrawText("LAYER: drag=add def  right-click=remove def  P=paint mode  Y=toggle type  V=vis  T=tex",
                     hx, hy, 10, (Color){ 160, 160, 255, 255 });
            (void)layer;
        }
    } else if (s->mode == MODE_BASE) {
        DrawText("Tab/1-4: Select block  |  Del: Remove  |  Left/Right: Weight (Shift=x5)  |  D+L/R: Density  |  Up/Down: tileScale",
                 hx, hy, 10, GRAY);
    } else if (s->detailPaintMode) {
        DrawText("DETAIL PAINT MODE: left-drag=paint  right-drag=erase  click panel=select brush  P=exit",
                 hx, hy, 10, (Color){ 100, 255, 200, 255 });
    } else if (s->activeDetailDef >= 0 && s->activeDetailDef < s->def.detailDefCount) {
        DrawText("Left/Right: Width ±1px (Shift=±8)  |  Up/Down: Height ±1px (Shift=±8)  |  Del: Remove selected  |  D+L/R: Density",
                 hx, hy, 10, SKYBLUE);
    } else {
        DrawText("Click=add tile  |  Drag=add region  |  Click existing=select  |  Right-click=remove  |  Del: Remove last  |  D+L/R: Density  |  P=paint mode",
                 hx, hy, 10, GRAY);
    }
    hy += 13;
    DrawText("G: Grid  |  M: Mode  |  [/]: GridW  |  Shift+[/]: GridH  |  F1-F5: 8/16/32/48/64  |  R: Regen  |  E: Export (unified)",
             hx, hy, 10, GRAY);
    hy += 13;
    DrawText("Ctrl+S: Save session  |  Ctrl+L: Load session",
             hx, hy, 10, (Color){ 120, 200, 120, 255 });
    hy += 13;
    DrawText("N: New layer  |  Ctrl+Tab: Cycle layer  |  Ctrl+Up/Down: Reorder  |  Ctrl+Del: Delete layer",
             hx, hy, 10, (Color){ 160, 160, 255, 255 });
    hy += 20;

    // Mode + settings row
    Color modeColor = (s->mode == MODE_BASE) ? GREEN : SKYBLUE;
    if (s->activeLayer >= 0) modeColor = (Color){ 160, 160, 255, 255 };
    DrawText(TextFormat("Mode: %-6s  tileScale: %.1f  Grid: %dx%d  Zoom: %.1fx  Seed: %d  cursor: (%.0f,%.0f)",
                        s->activeLayer >= 0 ? TextFormat("LAYER%d", s->activeLayer)
                            : (s->mode == MODE_BASE ? "BASE" : "DETAIL"),
                        s->def.tileScale, s->gridTileW, s->gridTileH,
                        s->zoom, s->seed,
                        s->cursorTex.x, s->cursorTex.y),
             hx, hy, FS, modeColor);
    hy += LH + 5;

    if (s->activeLayer < 0) {
        if (s->mode == MODE_BASE) {
            int totalW = 0;
            for (int i = 0; i < s->def.blockCount; i++) totalW += s->def.blockWeights[i];

            DrawText(TextFormat("BASE BLOCKS (%d / %d):%s",
                                s->def.blockCount, MAX_TILE_BLOCKS,
                                s->def.blockCount < 2 ? "  (add 2+ blocks for weights to matter)" : ""),
                     hx, hy, FS, s->def.blockCount < 2 ? ORANGE : WHITE);
            hy += LH;

            for (int i = 0; i < s->def.blockCount; i++) {
                TileBlock *blk = &s->def.blocks[i];
                bool active = (i == s->activeBlock);
                Color tc = active ? YELLOW : LIGHTGRAY;
                float pct = (totalW > 0) ? (float)s->def.blockWeights[i] / totalW : 0;

                DrawText(TextFormat("%c %d.  (%d,%d)  %dx%d  [%dx%d]  w:%d",
                                    active ? '>' : ' ', i,
                                    blk->srcX, blk->srcY, blk->cols, blk->rows,
                                    blk->tileW, blk->tileH, s->def.blockWeights[i]),
                         hx, hy, FS - 1, tc);

                int barX = hx + 340;
                int barW = 150;
                int barH = LH - 4;
                Color bc = BLOCK_COLORS[i % MAX_TILE_BLOCKS];
                DrawRectangle(barX, hy + 1, (int)(pct * barW), barH,
                              (Color){ bc.r, bc.g, bc.b, active ? 220 : 140 });
                DrawRectangleLines(barX, hy + 1, barW, barH, (Color){ 70, 70, 70, 255 });
                DrawText(TextFormat("%.0f%%", pct * 100.f),
                         barX + barW + 5, hy, FS - 1, active ? YELLOW : LIGHTGRAY);
                hy += LH;
            }
            DrawText(TextFormat("Tile defs compiled: %d  (total weight: %d)",
                                s->def.tileDefCount, totalW),
                     hx, hy, FS - 1, DARKGRAY);
            hy += LH;

        } else {
            DrawText(TextFormat("DETAIL MODE  —  Density: %d%%  (D+Left/Right)",
                                s->def.detailDensity),
                     hx, hy, FS, SKYBLUE);
            hy += LH;
            if (!s->def.detailLoaded) {
                DrawText("No detail texture (pass as 2nd arg)", hx, hy, FS - 1, DARKGRAY);
                hy += LH;
            } else {
                DrawText(TextFormat("Detail defs: %d / %d   "
                                    "click=add  drag=add region  right-click=remove  Del=delete",
                                    s->def.detailDefCount, MAX_DETAIL_DEFS),
                         hx, hy, FS - 1, LIGHTGRAY);
                hy += LH;
                if (s->activeDetailDef >= 0 && s->activeDetailDef < s->def.detailDefCount) {
                    Rectangle r = s->def.detailDefs[s->activeDetailDef].source;
                    DrawText(TextFormat("Selected: %d  (%.0f, %.0f)  %.0fx%.0f px",
                                        s->activeDetailDef, r.x, r.y, r.width, r.height),
                             hx, hy, FS - 1, YELLOW);
                    hy += LH;
                }
            }
        }
    }

    // Layer list
    {
        // Divider
        DrawLine(hx, hy, hx + PREVIEW_W - 10, hy, (Color){ 50, 50, 50, 255 });
        hy += 5;

        const char *activeStr = (s->activeLayer < 0) ? "base"
                                : TextFormat("Layer %d", s->activeLayer);
        DrawText(TextFormat("LAYERS (%d/%d)  active: %s  previewZoom:%.1fx",
                            s->layerCount, MAX_LAYERS, activeStr, s->previewZoom),
                 hx, hy, FS, (Color){ 180, 180, 255, 255 });
        hy += LH;

        for (int li = 0; li < s->layerCount; li++) {
            if (hy > WIN_H - 20) break;
            Layer *layer = &s->layers[li];
            bool isActive = (li == s->activeLayer);
            Color tc = isActive ? YELLOW : LIGHTGRAY;
            const char *paint = (isActive && s->paintMode) ? " [PAINT]" : "";
            DrawText(TextFormat("%c [%d] %-12s %-6s vis:%c defs:%-3d tex:%d density:%d%s",
                                isActive ? '>' : ' ', li, layer->name,
                                layer->type == LAYER_PAINT ? "PAINT" : "RANDOM",
                                layer->visible ? 'Y' : 'N',
                                layer->defCount, layer->texIdx, layer->density, paint),
                     hx, hy, FS - 1, tc);
            hy += LH;
        }

        if (s->paintMode && s->activeLayer >= 0 && s->brushDef >= 0) {
            if (hy <= WIN_H - 20)
                DrawText(TextFormat("brush: def[%d]", s->brushDef), hx, hy, FS - 1, (Color){100,255,200,255});
        }
    }

    // Status message (save/load feedback)
    if (s->statusTimer > 0) {
        float alpha = s->statusTimer < 1.0f ? s->statusTimer : 1.0f;
        Color sc = { 120, 255, 120, (unsigned char)(alpha * 255) };
        DrawText(s->statusMsg, hx, WIN_H - 18, 12, sc);
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    InitWindow(WIN_W, WIN_H, "Biome Preview Tool");
    SetTargetFPS(60);

    State s = {0};
    s.zoom              = 2.0f;
    s.showGrid          = true;
    s.gridTileW         = 32;
    s.gridTileH         = 32;
    s.def.tileScale     = 1.0f;
    s.def.detailDensity = 20;
    s.seed              = 42;
    s.activeBlock       = -1;
    s.activeDetailDef   = -1;
    s.activeLayer       = -1;
    s.brushDef          = -1;
    s.previewZoom       = 1.0f;
    strncpy(s.savePath, "biome_preview.save", sizeof s.savePath - 1);

    // Load all argv textures into pool
    for (int i = 1; i < argc && s.texCount < MAX_LAYERS; i++) {
        strncpy(s.texPaths[s.texCount], argv[i], 255);
        s.texPaths[s.texCount][255] = '\0';
        s.loadedTextures[s.texCount] = LoadTexture(argv[i]);
        SetTextureFilter(s.loadedTextures[s.texCount], TEXTURE_FILTER_POINT);
        printf("Loaded texture[%d]: %s  (%dx%d)\n",
               s.texCount, argv[i],
               s.loadedTextures[s.texCount].width,
               s.loadedTextures[s.texCount].height);
        s.texCount++;
    }

    // Wire def aliases into pool
    if (s.texCount >= 1) {
        s.def.texturePath = s.texPaths[0];
        s.def.texture     = s.loadedTextures[0];
        s.def.loaded      = true;
    }
    if (s.texCount >= 2) {
        s.def.detailTexturePath = s.texPaths[1];
        s.def.detailTexture     = s.loadedTextures[1];
        s.def.detailLoaded      = true;
    }

    while (!WindowShouldClose()) {

        /* ── Input ─────────────────────────────────────────────────────── */

        bool inPanel   = (GetMouseX() < PANEL_W);
        bool inPreview = (GetMouseX() >= PREVIEW_X && GetMouseX() < PREVIEW_X + PREVIEW_W &&
                          GetMouseY() >= PREVIEW_Y && GetMouseY() < PREVIEW_Y + PREVIEW_H);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        // ── Mode toggle (only in base editing)
        if (IsKeyPressed(KEY_M) && s.activeLayer < 0) {
            s.mode = (s.mode == MODE_BASE) ? MODE_DETAIL : MODE_BASE;
            s.activeBlock     = -1;
            s.activeDetailDef = -1;
            s.dragging        = false;
            s.detailPaintMode = false;
        }

        // ── Import biome (Ctrl + 1-4)
        if (ctrl) {
            if (IsKeyPressed(KEY_ONE))   import_biome(&s, BIOME_GRASS);
            if (IsKeyPressed(KEY_TWO))   import_biome(&s, BIOME_UNDEAD);
            if (IsKeyPressed(KEY_THREE)) import_biome(&s, BIOME_SNOW);
            if (IsKeyPressed(KEY_FOUR))  import_biome(&s, BIOME_SWAMP);
        }

        // ── Block selection: 1-4, Tab (base editing only)
        if (s.activeLayer < 0) {
            int bCount = (s.mode == MODE_BASE) ? s.def.blockCount : s.def.detailBlockCount;
            if (!ctrl) {
                for (int k = 0; k < 4; k++)
                    if (IsKeyPressed(KEY_ONE + k) && k < bCount) s.activeBlock = k;
            }
            if (IsKeyPressed(KEY_TAB) && bCount > 0) {
                int cur = (s.activeBlock < 0) ? 0 : s.activeBlock;
                s.activeBlock = wrap(cur + (shift ? -1 : 1), bCount);
            }
        }

        // ── Ctrl+Tab: Cycle active layer
        if (ctrl && IsKeyPressed(KEY_TAB)) {
            s.paintMode       = false;
            s.detailPaintMode = false;
            s.dragging        = false;
            int total = s.layerCount + 1; // -1→0, 0→1, ..., N-1→N
            if (total > 1) {
                int cur = s.activeLayer + 1;
                cur = wrap(cur + (shift ? -1 : 1), total);
                s.activeLayer = cur - 1;
            }
        }

        // ── Layer management keys
        // N: Add new layer
        if (IsKeyPressed(KEY_N) && !ctrl && s.layerCount < MAX_LAYERS) {
            Layer *layer = &s.layers[s.layerCount];
            memset(layer, 0, sizeof(Layer));
            snprintf(layer->name, sizeof(layer->name), "Layer %d", s.layerCount);
            layer->type    = LAYER_PAINT;
            layer->visible = true;
            layer->texIdx  = 0;
            layer->tileScale = s.def.tileScale;
            layer->density   = 20;
            s.activeLayer = s.layerCount++;
            s.paintMode   = false;
            s.brushDef    = -1;
            s.dragging    = false;
        }

        // V: Toggle layer visibility
        if (IsKeyPressed(KEY_V) && s.activeLayer >= 0 && s.activeLayer < s.layerCount) {
            s.layers[s.activeLayer].visible = !s.layers[s.activeLayer].visible;
        }

        // P: Toggle paint mode
        if (IsKeyPressed(KEY_P) && s.activeLayer >= 0 && s.activeLayer < s.layerCount) {
            Layer *layer = &s.layers[s.activeLayer];
            if (!s.paintMode) {
                // Lazily allocate paint cells
                if (!layer->paintCells && s.tmapValid) {
                    layer_alloc_cells(layer, s.tmap.rows, s.tmap.cols);
                }
                if (layer->defCount > 0) {
                    s.paintMode = true;
                    s.brushDef  = (layer->defCount > 0) ? 0 : -1;
                }
            } else {
                s.paintMode = false;
            }
        } else if (IsKeyPressed(KEY_P) && s.activeLayer < 0 && s.mode == MODE_DETAIL
                   && s.def.detailDefCount > 0) {
            if (!s.detailPaintMode) {
                if (!s.detailPaintCells && s.tmapValid) {
                    int n = s.tmap.rows * s.tmap.cols;
                    s.detailPaintCells = malloc(n * sizeof(int));
                    memset(s.detailPaintCells, 0xFF, (size_t)n * sizeof(int));
                    s.detailPaintRows = s.tmap.rows;
                    s.detailPaintCols = s.tmap.cols;
                }
                s.detailPaintMode = true;
                s.detailBrushDef  = 0;
            } else {
                s.detailPaintMode = false;
            }
        }

        // T: Cycle texIdx for active layer
        if (IsKeyPressed(KEY_T) && s.activeLayer >= 0 && s.texCount > 0) {
            Layer *layer = &s.layers[s.activeLayer];
            layer->texIdx = (layer->texIdx + 1) % s.texCount;
            Texture2D *tex = &s.loadedTextures[layer->texIdx];
            for (int j = 0; j < layer->defCount; j++)
                layer->defs[j].texture = tex;
            // Reset scroll so new texture is visible
            s.scrollX = 0; s.scrollY = 0;
        }

        // Y: Toggle layer type RANDOM/PAINT
        if (IsKeyPressed(KEY_Y) && s.activeLayer >= 0 && s.activeLayer < s.layerCount) {
            Layer *layer = &s.layers[s.activeLayer];
            layer->type = (layer->type == LAYER_RANDOM) ? LAYER_PAINT : LAYER_RANDOM;
            s.paintMode = false;
            s.dirty = true;
        }

        // Ctrl+Up: move active layer up (lower index)
        if (ctrl && IsKeyPressed(KEY_UP) && s.activeLayer > 0) {
            Layer tmp = s.layers[s.activeLayer];
            s.layers[s.activeLayer] = s.layers[s.activeLayer - 1];
            s.layers[s.activeLayer - 1] = tmp;
            s.activeLayer--;
        }

        // Ctrl+Down: move active layer down (higher index)
        if (ctrl && IsKeyPressed(KEY_DOWN) && s.activeLayer >= 0 &&
            s.activeLayer < s.layerCount - 1) {
            Layer tmp = s.layers[s.activeLayer];
            s.layers[s.activeLayer] = s.layers[s.activeLayer + 1];
            s.layers[s.activeLayer + 1] = tmp;
            s.activeLayer++;
        }

        // Ctrl+Delete: delete active layer
        if (ctrl && (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) &&
            s.activeLayer >= 0 && s.activeLayer < s.layerCount) {
            int idx = s.activeLayer;
            free(s.layers[idx].randCells);
            free(s.layers[idx].paintCells);
            for (int j = idx; j < s.layerCount - 1; j++)
                s.layers[j] = s.layers[j + 1];
            // Zero out the vacated slot
            memset(&s.layers[s.layerCount - 1], 0, sizeof(Layer));
            s.layerCount--;
            s.paintMode = false;
            s.brushDef  = -1;
            if (s.activeLayer >= s.layerCount)
                s.activeLayer = s.layerCount - 1;  // may become -1 if layerCount==0
        }

        #define HOLD_DELAY  0.35f
        #define HOLD_REPEAT 0.08f

        // ── Left/Right held repeat
        //    D held     → density ±5 (active layer or detail)
        //    BASE mode  → block weight ±5 (±25 Shift)
        //    DETAIL mode, def selected → detail def width ±1 (±8 Shift)
        {
            bool dHeld = IsKeyDown(KEY_D);
            int  dir   = 0;
            if (IsKeyDown(KEY_RIGHT)) dir =  1;
            if (IsKeyDown(KEY_LEFT))  dir = -1;

            if (dHeld) {
                bool fire = false;
                if (dir != s.densityHoldDir) {
                    s.densityHoldDir = dir; s.densityHoldTimer = 0; fire = (dir != 0);
                } else if (dir != 0) {
                    s.densityHoldTimer += GetFrameTime();
                    if (s.densityHoldTimer >= HOLD_DELAY) { s.densityHoldTimer -= HOLD_REPEAT; fire = true; }
                }
                if (fire) {
                    if (s.activeLayer >= 0 && s.activeLayer < s.layerCount) {
                        s.layers[s.activeLayer].density =
                            clampi(s.layers[s.activeLayer].density + dir * 5, 0, 100);
                    } else {
                        s.def.detailDensity = clampi(s.def.detailDensity + dir * 5, 0, 100);
                    }
                    s.dirty = true;
                }
                s.arrowHoldDir = 0; s.arrowHoldTimer = 0;
            } else {
                s.densityHoldDir = 0; s.densityHoldTimer = 0;

                bool fire = false;
                if (dir != s.arrowHoldDir) {
                    s.arrowHoldDir = dir; s.arrowHoldTimer = 0; fire = (dir != 0);
                } else if (dir != 0) {
                    s.arrowHoldTimer += GetFrameTime();
                    if (s.arrowHoldTimer >= HOLD_DELAY) { s.arrowHoldTimer -= HOLD_REPEAT; fire = true; }
                }
                if (fire && s.activeLayer < 0) {
                    bool detailDefActive = s.mode == MODE_DETAIL &&
                                          s.activeDetailDef >= 0 &&
                                          s.activeDetailDef < s.def.detailDefCount;
                    bool baseBlockActive = s.mode == MODE_BASE &&
                                          s.activeBlock >= 0 &&
                                          s.activeBlock < s.def.blockCount;
                    if (detailDefActive) {
                        int step = shift ? 8 : 1;
                        Rectangle *r = &s.def.detailDefs[s.activeDetailDef].source;
                        r->width = fmaxf(1.f, r->width + dir * step);
                        s.dirty = true;
                    } else if (baseBlockActive) {
                        int step = shift ? 25 : 5;
                        s.def.blockWeights[s.activeBlock] =
                            clampi(s.def.blockWeights[s.activeBlock] + dir * step, 1, 9999);
                        s.dirty = true;
                    }
                }
            }
        }

        // ── Up/Down held repeat
        {
            bool detailDefActive = s.activeLayer < 0 &&
                                   s.mode == MODE_DETAIL &&
                                   s.activeDetailDef >= 0 &&
                                   s.activeDetailDef < s.def.detailDefCount;
            if (detailDefActive) {
                int dir = 0;
                if (IsKeyDown(KEY_DOWN)) dir =  1;
                if (IsKeyDown(KEY_UP))   dir = -1;

                bool fire = false;
                if (dir != s.udHoldDir) {
                    s.udHoldDir = dir; s.udHoldTimer = 0; fire = (dir != 0);
                } else if (dir != 0) {
                    s.udHoldTimer += GetFrameTime();
                    if (s.udHoldTimer >= HOLD_DELAY) { s.udHoldTimer -= HOLD_REPEAT; fire = true; }
                }
                if (fire) {
                    int step = shift ? 8 : 1;
                    Rectangle *r = &s.def.detailDefs[s.activeDetailDef].source;
                    r->height = fmaxf(1.f, r->height + dir * step);
                    s.dirty = true;
                }
            } else if (!ctrl) {
                // tileScale (only without ctrl, which is used for layer reorder)
                s.udHoldDir = 0; s.udHoldTimer = 0;
                if (s.activeLayer < 0) {
                    if (IsKeyPressed(KEY_UP))   { s.def.tileScale = clampf(s.def.tileScale + 0.5f, 0.5f, 8.0f); s.dirty = true; }
                    if (IsKeyPressed(KEY_DOWN)) { s.def.tileScale = clampf(s.def.tileScale - 0.5f, 0.5f, 8.0f); s.dirty = true; }
                }
            }
        }

        // ── Delete (non-ctrl: base block / detail def)
        if (!ctrl && (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE))) {
            if (s.activeLayer < 0) {
                if (s.mode == MODE_BASE && s.activeBlock >= 0 && s.def.blockCount > 0) {
                    int idx = s.activeBlock;
                    for (int j = idx; j < s.def.blockCount - 1; j++) {
                        s.def.blocks[j]       = s.def.blocks[j + 1];
                        s.def.blockWeights[j] = s.def.blockWeights[j + 1];
                    }
                    s.def.blockCount--;
                    s.activeBlock = (s.def.blockCount > 0) ? clampi(idx, 0, s.def.blockCount - 1) : -1;
                    recompile(&s);
                    s.dirty = true;

                } else if (s.mode == MODE_DETAIL && s.def.detailDefCount > 0) {
                    int idx = (s.activeDetailDef >= 0 && s.activeDetailDef < s.def.detailDefCount)
                              ? s.activeDetailDef : s.def.detailDefCount - 1;
                    for (int j = idx; j < s.def.detailDefCount - 1; j++)
                        s.def.detailDefs[j] = s.def.detailDefs[j + 1];
                    s.def.detailDefCount--;
                    for (int j = 0; j < s.def.detailDefCount; j++)
                        s.def.detailDefs[j].texture = &s.def.detailTexture;
                    s.activeDetailDef = (s.def.detailDefCount > 0)
                        ? clampi(idx, 0, s.def.detailDefCount - 1) : -1;
                    s.dirty = true;
                }
            }
        }

        // ── Grid tile size  ([/] width ±1, Shift+[/] height ±1, hold to repeat)
        {
            bool bL  = IsKeyDown(KEY_LEFT_BRACKET);
            bool bR  = IsKeyDown(KEY_RIGHT_BRACKET);
            int  dir = bL ? -1 : (bR ? 1 : 0);
            bool isH = shift && (bL || bR);
            if (dir == 0) {
                s.sizeHoldKey = 0; s.sizeHoldTimer = 0;
            } else {
                bool fire = false;
                if (dir != s.sizeHoldKey || isH != s.sizeHoldIsH) {
                    s.sizeHoldKey = dir; s.sizeHoldIsH = isH;
                    s.sizeHoldTimer = 0; fire = true;
                } else {
                    s.sizeHoldTimer += GetFrameTime();
                    if (s.sizeHoldTimer >= HOLD_DELAY) { s.sizeHoldTimer -= HOLD_REPEAT; fire = true; }
                }
                if (fire) {
                    if (isH) s.gridTileH = clampi(s.gridTileH + dir, 1, 256);
                    else     s.gridTileW = clampi(s.gridTileW + dir, 1, 256);
                }
            }
        }

        // ── Grid size presets
        {
            static const int SIZE_PRESETS[] = { 8, 16, 32, 48, 64 };
            for (int k = 0; k < 5; k++) {
                if (IsKeyPressed(KEY_F1 + k)) {
                    s.gridTileW = SIZE_PRESETS[k];
                    s.gridTileH = SIZE_PRESETS[k];
                }
            }
        }

        // ── Zoom (tileset panel)
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
            s.zoom = clampf(s.zoom + 0.5f, 0.5f, 8.0f);
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
            s.zoom = clampf(s.zoom - 0.5f, 0.5f, 8.0f);

        // ── Grid toggle
        if (IsKeyPressed(KEY_G)) s.showGrid = !s.showGrid;

        // ── Regenerate
        if (IsKeyPressed(KEY_R)) { s.seed += 7; s.dirty = true; }

        // ── Export (unified: base + detail + all layers)
        if (!ctrl && IsKeyPressed(KEY_E)) {
            printf("── Biome C Code ──────────────────────────────────────\n");
            export_c_code(&s);
            printf("──────────────────────────────────────────────────────\n\n");
        }

        // ── Save / Load
        if (ctrl && IsKeyPressed(KEY_S)) save_state(&s, s.savePath);
        if (ctrl && IsKeyPressed(KEY_L)) load_state(&s, s.savePath);

        // ── Status message timer
        if (s.statusTimer > 0) s.statusTimer -= GetFrameTime();

        // ── Preview zoom (Ctrl+Scroll on preview)
        if (inPreview && ctrl) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0)
                s.previewZoom = clampf(s.previewZoom + wheel * 0.25f, 0.5f, 4.0f);
        }

        // ── Scrolling (tileset panel, non-ctrl)
        if (inPanel && s.def.loaded && !ctrl) {
            float scrollSpeed = 40.0f;
            float wheelY = GetMouseWheelMove();
            if (shift) {
                s.scrollX -= wheelY * scrollSpeed;
            } else {
                s.scrollY -= wheelY * scrollSpeed;
            }
            Vector2 wheelV = GetMouseWheelMoveV();
            if (wheelV.x != 0) s.scrollX -= wheelV.x * scrollSpeed;

            Texture2D *tex = panel_tex(&s);
            if (tex && tex->id != 0) {
                s.scrollX = clampf(s.scrollX, 0, max_scroll(tex->width,  PANEL_W, s.zoom));
                s.scrollY = clampf(s.scrollY, 0, max_scroll(tex->height, WIN_H,   s.zoom));
            }
        }

        // ── Tileset panel middle mouse pan
        if (inPanel) {
            if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
                s.panning    = true;
                s.panOrigin  = GetMousePosition();
                s.panScrollX = s.scrollX;
                s.panScrollY = s.scrollY;
            }
        }
        if (s.panning) {
            if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                Vector2 mp = GetMousePosition();
                s.scrollX = s.panScrollX - (mp.x - s.panOrigin.x);
                s.scrollY = s.panScrollY - (mp.y - s.panOrigin.y);
                if (s.def.loaded) {
                    Texture2D *tex = panel_tex(&s);
                    if (tex && tex->id != 0) {
                        s.scrollX = clampf(s.scrollX, 0, max_scroll(tex->width,  PANEL_W, s.zoom));
                        s.scrollY = clampf(s.scrollY, 0, max_scroll(tex->height, WIN_H,   s.zoom));
                    }
                }
            } else {
                s.panning = false;
            }
        }

        // ── Preview middle mouse pan
        if (inPreview && !s.panning) {
            if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
                s.previewPanning    = true;
                s.previewPanOrigin  = GetMousePosition();
                s.previewPanStartX  = s.previewPanX;
                s.previewPanStartY  = s.previewPanY;
            }
        }
        if (s.previewPanning) {
            if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                Vector2 mp = GetMousePosition();
                s.previewPanX = s.previewPanStartX + (mp.x - s.previewPanOrigin.x);
                s.previewPanY = s.previewPanStartY + (mp.y - s.previewPanOrigin.y);
            } else {
                s.previewPanning = false;
            }
        }

        // ── Mouse: tileset panel — define blocks / select / add defs
        if (inPanel && !s.panning) {
            bool panelHasContent = (s.activeLayer >= 0) ? true : s.def.loaded;
            if (panelHasContent) {
                Vector2 mp      = GetMousePosition();
                Vector2 texPos  = screen_to_tex(mp.x, mp.y, s.scrollX, s.scrollY, s.zoom);
                s.cursorTex     = texPos;
                Vector2 snapped = snap_grid(texPos, s.gridTileW, s.gridTileH);

                if (s.activeLayer >= 0 && s.activeLayer < s.layerCount) {
                    Layer *layer = &s.layers[s.activeLayer];

                    if (s.paintMode) {
                        // paintMode: left-click to select brush def from panel
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            for (int j = 0; j < layer->defCount; j++) {
                                Rectangle r = layer->defs[j].source;
                                float dx = r.x * s.zoom - s.scrollX;
                                float dy = r.y * s.zoom - s.scrollY;
                                float dw = r.width  * s.zoom;
                                float dh = r.height * s.zoom;
                                if (mp.x >= dx && mp.x < dx + dw &&
                                    mp.y >= dy && mp.y < dy + dh) {
                                    s.brushDef = j;
                                    break;
                                }
                            }
                        }
                    } else {
                        // Not paintMode: drag creates TileDef, right-click removes
                        Texture2D *tex = (s.texCount > layer->texIdx)
                                         ? &s.loadedTextures[layer->texIdx] : NULL;

                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            s.dragging  = true;
                            s.dragStart = snapped;
                            s.dragCur   = snapped;
                        }
                        if (s.dragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                            s.dragCur = snapped;

                        if (s.dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                            s.dragging = false;
                            float x0  = fminf(s.dragStart.x, s.dragCur.x);
                            float y0  = fminf(s.dragStart.y, s.dragCur.y);
                            float x1  = fmaxf(s.dragStart.x, s.dragCur.x);
                            float y1  = fmaxf(s.dragStart.y, s.dragCur.y);
                            float selW = (x1 - x0) + s.gridTileW;
                            float selH = (y1 - y0) + s.gridTileH;
                            if (layer->defCount < MAX_LAYER_DEFS) {
                                layer->defs[layer->defCount] = (TileDef){
                                    .texture = tex,
                                    .source  = (Rectangle){ x0, y0, selW, selH }
                                };
                                layer->defCount++;
                                s.dirty = true;
                            }
                        }

                        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                            if (s.dragging) {
                                s.dragging = false;
                            } else {
                                for (int j = layer->defCount - 1; j >= 0; j--) {
                                    Rectangle r = layer->defs[j].source;
                                    if (texPos.x >= r.x && texPos.x < r.x + r.width &&
                                        texPos.y >= r.y && texPos.y < r.y + r.height) {
                                        for (int k = j; k < layer->defCount - 1; k++)
                                            layer->defs[k] = layer->defs[k + 1];
                                        layer->defCount--;
                                        // Rewire texture pointers
                                        for (int k = 0; k < layer->defCount; k++)
                                            layer->defs[k].texture = tex;
                                        if (s.brushDef >= layer->defCount)
                                            s.brushDef = layer->defCount - 1;
                                        s.dirty = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                } else if (s.def.loaded) {
                    // Base editing mode (activeLayer == -1)

                    if (s.detailPaintMode && s.mode == MODE_DETAIL) {
                        // Detail paint: left-click selects brush from panel
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            for (int j = 0; j < s.def.detailDefCount; j++) {
                                Rectangle r = s.def.detailDefs[j].source;
                                float dx = r.x * s.zoom - s.scrollX;
                                float dy = r.y * s.zoom - s.scrollY;
                                float dw = r.width  * s.zoom;
                                float dh = r.height * s.zoom;
                                if (mp.x >= dx && mp.x < dx + dw &&
                                    mp.y >= dy && mp.y < dy + dh) {
                                    s.detailBrushDef = j;
                                    break;
                                }
                            }
                        }
                        // Skip regular drag/add/remove
                    } else {
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            s.dragging  = true;
                            s.dragStart = snapped;
                            s.dragCur   = snapped;
                        }
                        if (s.dragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                            s.dragCur = snapped;

                    if (s.dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                        s.dragging = false;

                        float x0   = fminf(s.dragStart.x, s.dragCur.x);
                        float y0   = fminf(s.dragStart.y, s.dragCur.y);
                        float x1   = fmaxf(s.dragStart.x, s.dragCur.x);
                        float y1   = fmaxf(s.dragStart.y, s.dragCur.y);
                        float selW  = (x1 - x0) + s.gridTileW;
                        float selH  = (y1 - y0) + s.gridTileH;
                        int cols    = (int)roundf(selW / s.gridTileW);
                        int rows    = (int)roundf(selH / s.gridTileH);
                        bool isDetail = (s.mode == MODE_DETAIL);
                        bool isClick  = (s.dragStart.x == s.dragCur.x && s.dragStart.y == s.dragCur.y);

                        if (isDetail && s.def.detailLoaded) {
                            if (isClick) {
                                bool selected = false;
                                for (int j = 0; j < s.def.detailDefCount; j++) {
                                    Rectangle r = s.def.detailDefs[j].source;
                                    if (texPos.x >= r.x && texPos.x < r.x + r.width &&
                                        texPos.y >= r.y && texPos.y < r.y + r.height) {
                                        s.activeDetailDef = j;
                                        selected = true;
                                        break;
                                    }
                                }
                                if (!selected && s.def.detailDefCount < MAX_DETAIL_DEFS) {
                                    s.def.detailDefs[s.def.detailDefCount] = (TileDef){
                                        .texture = &s.def.detailTexture,
                                        .source  = (Rectangle){ x0, y0,
                                                                (float)s.gridTileW,
                                                                (float)s.gridTileH }
                                    };
                                    s.activeDetailDef = s.def.detailDefCount++;
                                    s.dirty = true;
                                }
                            } else {
                                if (s.def.detailDefCount < MAX_DETAIL_DEFS) {
                                    s.def.detailDefs[s.def.detailDefCount] = (TileDef){
                                        .texture = &s.def.detailTexture,
                                        .source  = (Rectangle){ x0, y0, selW, selH }
                                    };
                                    s.activeDetailDef = s.def.detailDefCount++;
                                    s.dirty = true;
                                }
                            }

                        } else if (!isDetail) {
                            if (s.def.blockCount < MAX_TILE_BLOCKS) {
                                TileBlock *blk = &s.def.blocks[s.def.blockCount];
                                blk->srcX  = (int)x0;  blk->srcY  = (int)y0;
                                blk->cols  = cols;      blk->rows  = rows;
                                blk->tileW = s.gridTileW; blk->tileH = s.gridTileH;
                                s.def.blockWeights[s.def.blockCount] = 50;
                                s.activeBlock = s.def.blockCount++;
                                recompile(&s);
                                s.dirty = true;
                            }
                        }
                    }

                    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                        if (s.dragging) {
                            s.dragging = false;
                        } else if (s.mode == MODE_DETAIL && s.def.detailLoaded) {
                            for (int j = s.def.detailDefCount - 1; j >= 0; j--) {
                                Rectangle r = s.def.detailDefs[j].source;
                                if (texPos.x >= r.x && texPos.x < r.x + r.width &&
                                    texPos.y >= r.y && texPos.y < r.y + r.height) {
                                    for (int k = j; k < s.def.detailDefCount - 1; k++)
                                        s.def.detailDefs[k] = s.def.detailDefs[k + 1];
                                    s.def.detailDefCount--;
                                    for (int k = 0; k < s.def.detailDefCount; k++)
                                        s.def.detailDefs[k].texture = &s.def.detailTexture;
                                    if (s.activeDetailDef >= s.def.detailDefCount)
                                        s.activeDetailDef = s.def.detailDefCount - 1;
                                    s.dirty = true;
                                    break;
                                }
                            }
                        }
                    }
                    } // end else (not detailPaintMode)
                }
            }
        }

        // ── Right panel: paint interaction
        s.hoveredPaintActive = false;
        if (s.paintMode && s.activeLayer >= 0 && s.activeLayer < s.layerCount &&
            s.tmapValid && inPreview) {
            Layer *layer = &s.layers[s.activeLayer];
            Vector2 mp = GetMousePosition();

            // screen → cell mapping (inverse of camera transform)
            float cellX = (mp.x - PREVIEW_X - s.previewPanX) / (s.tmap.tileSize * s.previewZoom);
            float cellY = (mp.y - PREVIEW_Y - s.previewPanY) / (s.tmap.tileSize * s.previewZoom);
            int pcol = (int)cellX;
            int prow = (int)cellY;

            if (prow >= 0 && prow < s.tmap.rows && pcol >= 0 && pcol < s.tmap.cols) {
                s.hoveredPaintRow    = prow;
                s.hoveredPaintCol    = pcol;
                s.hoveredPaintActive = true;

                // Ensure paint cells allocated
                if (!layer->paintCells)
                    layer_alloc_cells(layer, s.tmap.rows, s.tmap.cols);

                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && s.brushDef >= 0) {
                    layer->paintCells[prow * s.tmap.cols + pcol] = s.brushDef;
                }
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
                    layer->paintCells[prow * s.tmap.cols + pcol] = -1;
                }
            }
        }

        // ── Right panel: detail paint interaction
        if (s.detailPaintMode && s.activeLayer < 0 && s.mode == MODE_DETAIL
            && s.tmapValid && inPreview) {
            Vector2 mp = GetMousePosition();
            float cellX = (mp.x - PREVIEW_X - s.previewPanX) / (s.tmap.tileSize * s.previewZoom);
            float cellY = (mp.y - PREVIEW_Y - s.previewPanY) / (s.tmap.tileSize * s.previewZoom);
            int pcol = (int)cellX;
            int prow = (int)cellY;
            if (prow >= 0 && prow < s.tmap.rows && pcol >= 0 && pcol < s.tmap.cols) {
                s.hoveredPaintRow    = prow;
                s.hoveredPaintCol    = pcol;
                s.hoveredPaintActive = true;
                if (!s.detailPaintCells) {
                    int n = s.tmap.rows * s.tmap.cols;
                    s.detailPaintCells = malloc(n * sizeof(int));
                    memset(s.detailPaintCells, 0xFF, (size_t)n * sizeof(int));
                    s.detailPaintRows = s.tmap.rows;
                    s.detailPaintCols = s.tmap.cols;
                }
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && s.detailBrushDef >= 0)
                    s.detailPaintCells[prow * s.tmap.cols + pcol] = s.detailBrushDef;
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
                    s.detailPaintCells[prow * s.tmap.cols + pcol] = -1;
            }
        }

        // ── Regen tilemap
        if (s.dirty) { s.dirty = false; regen_tilemap(&s); }

        /* ── Draw ─────────────────────────────────────────────────────── */

        BeginDrawing();
        ClearBackground((Color){ 18, 18, 18, 255 });

        draw_tileset_panel(&s, inPanel);

        // Preview panel background
        DrawRectangle(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H,
                      (Color){ 38, 38, 38, 255 });

        if (s.tmapValid) {
            // Set up camera for zoom/pan
            Camera2D previewCam = {
                .offset = (Vector2){ PREVIEW_X + s.previewPanX, PREVIEW_Y + s.previewPanY },
                .target = (Vector2){ PREVIEW_X, PREVIEW_Y },
                .zoom   = s.previewZoom
            };

            BeginScissorMode(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H);
            BeginMode2D(previewCam);

            tilemap_draw(&s.tmap, s.def.tileDefs);
            if (s.def.detailDefCount > 0) {
                int *savedCells = s.tmap.detailCells;
                if (s.detailPaintCells) s.tmap.detailCells = s.detailPaintCells;
                tilemap_draw_details(&s.tmap, s.def.detailDefs);
                s.tmap.detailCells = savedCells;
            }
            for (int i = 0; i < s.layerCount; i++) {
                if (s.layers[i].visible)
                    layer_draw(&s.layers[i], &s.tmap);
            }

            EndMode2D();

            // Hover highlight in screen coords (inside scissor, after EndMode2D)
            if (((s.paintMode && s.activeLayer >= 0) || s.detailPaintMode) && s.hoveredPaintActive) {
                float csx = PREVIEW_X + s.previewPanX +
                            s.hoveredPaintCol * s.tmap.tileSize * s.previewZoom;
                float csy = PREVIEW_Y + s.previewPanY +
                            s.hoveredPaintRow * s.tmap.tileSize * s.previewZoom;
                float csw = s.tmap.tileSize * s.previewZoom;
                DrawRectangle((int)csx, (int)csy, (int)csw, (int)csw,
                              (Color){ 255, 255, 0, 60 });
                DrawRectangleLines((int)csx, (int)csy, (int)csw, (int)csw, YELLOW);
            }

            EndScissorMode();
        } else if (!s.def.loaded && s.texCount == 0) {
            DrawText("Load a tileset:  ./biome_preview base.png [detail.png] [layer.png]...",
                     PREVIEW_X + 16, PREVIEW_Y + 16, 15, GRAY);
        } else {
            DrawText("Drag on tileset to define tile blocks",
                     PREVIEW_X + 16, PREVIEW_Y + 16, 15, GRAY);
        }

        DrawRectangleLines(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H,
                           (Color){ 55, 55, 55, 255 });
        DrawText("TILEMAP PREVIEW", PREVIEW_X + 5, PREVIEW_Y + 4, 10,
                 (Color){ 80, 80, 80, 255 });
        if (s.paintMode && s.activeLayer >= 0) {
            DrawText(TextFormat("PAINT MODE — Layer %d — brush: def[%d]  (Ctrl+Scroll=zoom  Mid=pan)",
                                s.activeLayer, s.brushDef),
                     PREVIEW_X + 5, PREVIEW_Y + PREVIEW_H - 16, 10,
                     (Color){ 100, 255, 200, 200 });
        } else if (s.detailPaintMode) {
            DrawText(TextFormat("DETAIL PAINT MODE — brush: def[%d]  (Ctrl+Scroll=zoom  Mid=pan)",
                                s.detailBrushDef),
                     PREVIEW_X + 5, PREVIEW_Y + PREVIEW_H - 16, 10,
                     (Color){ 100, 255, 200, 200 });
        }

        draw_hud(&s);

        EndDrawing();
    }

    // Cleanup
    if (s.tmapValid) tilemap_free(&s.tmap);

    for (int i = 0; i < s.layerCount; i++) {
        free(s.layers[i].randCells);
        free(s.layers[i].paintCells);
    }
    free(s.detailPaintCells);

    // Unload pool textures; also unload any non-pool textures that biome imports loaded
    if (s.def.loaded && !tex_is_in_pool(&s, s.def.texture))
        UnloadTexture(s.def.texture);
    if (s.def.detailLoaded && !tex_is_in_pool(&s, s.def.detailTexture))
        UnloadTexture(s.def.detailTexture);
    for (int i = 0; i < s.texCount; i++)
        UnloadTexture(s.loadedTextures[i]);

    CloseWindow();
    return 0;
}
