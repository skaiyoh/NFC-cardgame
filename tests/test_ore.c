/*
 * Unit tests for src/core/ore.c
 *
 * Self-contained: redefines minimal type stubs and includes ore.c,
 * battlefield.c, and battlefield_math.c directly. Compiles with only -lm.
 *
 * Uses the header-guard-override pattern from test_battlefield.c.
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ---- Prevent heavy header inclusion ---- */
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_TILEMAP_RENDERER_H
#define NFC_CARDGAME_BIOME_H
#define NFC_CARDGAME_TYPES_H

/* ---- Config defines (must match src/core/config.h) ---- */
#define BOARD_WIDTH        1080
#define BOARD_HEIGHT       1920
#define SEAM_Y             960
#define BASE_SPAWN_GAP     32.0f
#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.3f
#define LANE_OUTER_INSET_RATIO 0.25f
#define LANE_BASE_APPROACH_START 0.72f
#define LANE_BASE_APPROACH_GAP 16.0f
#define PI_F               3.14159265f
#define NUM_CARD_SLOTS     3
#define MAX_ENTITIES       64
#define TILE_COUNT         32
#define MAX_DETAIL_DEFS    64

/* Ore config (must match src/core/config.h) */
#define ORE_GRID_CELL_SIZE_PX        64.0f
#define ORE_GRID_COLS                16
#define ORE_GRID_ROWS                15
#define ORE_EDGE_MARGIN_CELLS        1
#define ORE_LANE_CLEARANCE_CELLS     1.0f
#define ORE_BASE_CLEARANCE_CELLS     2.0f
#define ORE_SPAWN_CLEARANCE_CELLS    1.5f
#define ORE_NODE_CLEARANCE_CELLS     1.5f
#define ORE_MATCH_COUNT_PER_SIDE     8

/* ---- Minimal type stubs ---- */
#define VECTOR2_DEFINED
typedef struct { float x; float y; } Vector2;

typedef struct { float x, y, width, height; } Rectangle;

typedef enum {
    BIOME_GRASS,
    BIOME_UNDEAD,
    BIOME_SNOW,
    BIOME_SWAMP,
    BIOME_COUNT
} BiomeType;

typedef struct {
    void *texture;
    Rectangle source;
} TileDef;

typedef struct {
    int rows, cols;
    int *cells;
    int *detailCells;
    void *biomeLayerCells[8];
    float tileSize;
    float tileScale;
    float originX, originY;
} TileMap;

typedef struct BiomeDef {
    const char *texturePath;
    void *texture;
    int loaded;
    TileDef tileDefs[TILE_COUNT];
    int tileDefCount;
    TileDef detailDefs[MAX_DETAIL_DEFS];
    int detailDefCount;
} BiomeDef;

typedef struct {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;

/* ---- Minimal Entity stub ---- */
typedef struct Entity {
    int id;
} Entity;

/* ---- Stub functions ---- */

TileMap tilemap_create_biome(Rectangle area, float tileSize, unsigned int seed,
                             const BiomeDef *biome) {
    (void)area; (void)tileSize; (void)seed; (void)biome;
    TileMap map;
    memset(&map, 0, sizeof(TileMap));
    return map;
}

void tilemap_free(TileMap *map) { (void)map; }

void biome_copy_tiledefs(const BiomeDef *biome, TileDef outDefs[TILE_COUNT]) {
    (void)biome; (void)outDefs;
}

void biome_copy_detail_defs(const BiomeDef *biome, TileDef outDefs[MAX_DETAIL_DEFS]) {
    (void)biome; (void)outDefs;
}

int biome_tile_count(const BiomeDef *biome) {
    (void)biome;
    return 0;
}

/* ---- Include production code under test ---- */
#include "../src/core/battlefield_math.c"
#include "../src/core/battlefield.c"
#include "../src/core/ore.c"

/* ---- Test helpers ---- */

static const uint32_t TEST_SEED = 12345;

static Battlefield create_test_bf(void) {
    BiomeDef defs[BIOME_COUNT];
    memset(defs, 0, sizeof(defs));
    Battlefield bf;
    bf_init(&bf, defs, BIOME_GRASS, BIOME_GRASS, 64.0f, 42, 99);
    ore_init(&bf, TEST_SEED);
    return bf;
}

/* Point-to-segment distance (mirrors ore.c's static function for testing) */
static float test_pt_seg_dist(Vector2 p, Vector2 a, Vector2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-8f) {
        float ex = p.x - a.x;
        float ey = p.y - a.y;
        return sqrtf(ex * ex + ey * ey);
    }
    float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float projX = a.x + t * dx;
    float projY = a.y + t * dy;
    float ex = p.x - projX;
    float ey = p.y - projY;
    return sqrtf(ex * ex + ey * ey);
}

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

/* ---- Tests ---- */

static void test_init_bottom_count(void) {
    Battlefield bf = create_test_bf();
    assert(ore_active_count(&bf, SIDE_BOTTOM) == ORE_MATCH_COUNT_PER_SIDE);
    printf("  PASS: test_init_bottom_count\n");
}

static void test_init_top_count(void) {
    Battlefield bf = create_test_bf();
    assert(ore_active_count(&bf, SIDE_TOP) == ORE_MATCH_COUNT_PER_SIDE);
    printf("  PASS: test_init_top_count\n");
}

static void test_all_bottom_in_territory(void) {
    Battlefield bf = create_test_bf();
    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
        OreNode *n = &bf.oreField.nodes[SIDE_BOTTOM][i];
        assert(n->active);
        assert(n->worldPos.v.y >= (float)SEAM_Y);
        assert(n->worldPos.v.y <= (float)BOARD_HEIGHT);
    }
    printf("  PASS: test_all_bottom_in_territory\n");
}

static void test_all_top_in_territory(void) {
    Battlefield bf = create_test_bf();
    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
        OreNode *n = &bf.oreField.nodes[SIDE_TOP][i];
        assert(n->active);
        assert(n->worldPos.v.y >= 0.0f);
        assert(n->worldPos.v.y <= (float)SEAM_Y);
    }
    printf("  PASS: test_all_top_in_territory\n");
}

static void test_ore_obeys_edge_margin(void) {
    Battlefield bf = create_test_bf();
    float minX = ORE_GRID_ORIGIN_X + ORE_EDGE_MARGIN_CELLS * ORE_GRID_CELL_SIZE_PX;
    float maxX = ORE_GRID_ORIGIN_X + (ORE_GRID_COLS - ORE_EDGE_MARGIN_CELLS) * ORE_GRID_CELL_SIZE_PX;

    for (int s = 0; s < 2; s++) {
        float territoryY = (s == SIDE_BOTTOM) ? (float)SEAM_Y : 0.0f;
        float minY = territoryY + ORE_EDGE_MARGIN_CELLS * ORE_GRID_CELL_SIZE_PX;
        float maxY = territoryY + (ORE_GRID_ROWS - ORE_EDGE_MARGIN_CELLS) * ORE_GRID_CELL_SIZE_PX;
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
            OreNode *n = &bf.oreField.nodes[s][i];
            assert(n->worldPos.v.x >= minX && n->worldPos.v.x <= maxX);
            assert(n->worldPos.v.y >= minY && n->worldPos.v.y <= maxY);
        }
    }
    printf("  PASS: test_ore_obeys_edge_margin\n");
}

static void test_ore_clears_lane_geometry(void) {
    Battlefield bf = create_test_bf();
    float laneClearPx = ORE_LANE_CLEARANCE_CELLS * ORE_GRID_CELL_SIZE_PX;

    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
            OreNode *n = &bf.oreField.nodes[s][i];
            for (int ls = 0; ls < 2; ls++) {
                for (int lane = 0; lane < 3; lane++) {
                    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
                        CanonicalPos a = bf_waypoint(&bf, (BattleSide)ls, lane, wp);
                        CanonicalPos b = bf_waypoint(&bf, (BattleSide)ls, lane, wp + 1);
                        float dist = test_pt_seg_dist(n->worldPos.v, a.v, b.v);
                        assert(dist >= laneClearPx - 0.01f);
                    }
                }
            }
        }
    }
    printf("  PASS: test_ore_clears_lane_geometry\n");
}

static void test_ore_clears_base_anchor(void) {
    Battlefield bf = create_test_bf();
    float baseClearPx = ORE_BASE_CLEARANCE_CELLS * ORE_GRID_CELL_SIZE_PX;

    for (int s = 0; s < 2; s++) {
        CanonicalPos base = bf_base_anchor(&bf, (BattleSide)s);
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
            OreNode *n = &bf.oreField.nodes[s][i];
            float dist = bf_distance(n->worldPos, base);
            assert(dist >= baseClearPx - 0.01f);
        }
    }
    printf("  PASS: test_ore_clears_base_anchor\n");
}

static void test_ore_clears_spawn_anchors(void) {
    Battlefield bf = create_test_bf();
    float spawnClearPx = ORE_SPAWN_CLEARANCE_CELLS * ORE_GRID_CELL_SIZE_PX;

    for (int s = 0; s < 2; s++) {
        for (int slot = 0; slot < NUM_CARD_SLOTS; slot++) {
            CanonicalPos anchor = bf_spawn_pos(&bf, (BattleSide)s, slot);
            for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
                OreNode *n = &bf.oreField.nodes[s][i];
                float dist = bf_distance(n->worldPos, anchor);
                assert(dist >= spawnClearPx - 0.01f);
            }
        }
    }
    printf("  PASS: test_ore_clears_spawn_anchors\n");
}

static void test_ore_no_overlapping_nodes(void) {
    Battlefield bf = create_test_bf();
    float nodeClearPx = ORE_NODE_CLEARANCE_CELLS * ORE_GRID_CELL_SIZE_PX;

    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
            for (int j = i + 1; j < ORE_MATCH_COUNT_PER_SIDE; j++) {
                OreNode *a = &bf.oreField.nodes[s][i];
                OreNode *b = &bf.oreField.nodes[s][j];
                float dist = bf_distance(a->worldPos, b->worldPos);
                assert(dist >= nodeClearPx - 0.01f);
            }
        }
    }
    printf("  PASS: test_ore_no_overlapping_nodes\n");
}

static void test_deterministic_same_seed(void) {
    Battlefield bf1 = create_test_bf();
    Battlefield bf2 = create_test_bf();

    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
            assert(bf1.oreField.nodes[s][i].gridRow == bf2.oreField.nodes[s][i].gridRow);
            assert(bf1.oreField.nodes[s][i].gridCol == bf2.oreField.nodes[s][i].gridCol);
        }
    }
    printf("  PASS: test_deterministic_same_seed\n");
}

static void test_different_seed_changes_placement(void) {
    Battlefield bf1 = create_test_bf();

    BiomeDef defs[BIOME_COUNT];
    memset(defs, 0, sizeof(defs));
    Battlefield bf2;
    bf_init(&bf2, defs, BIOME_GRASS, BIOME_GRASS, 64.0f, 42, 99);
    ore_init(&bf2, TEST_SEED + 1);

    bool anyDifferent = false;
    for (int s = 0; s < 2 && !anyDifferent; s++) {
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE && !anyDifferent; i++) {
            if (bf1.oreField.nodes[s][i].gridRow != bf2.oreField.nodes[s][i].gridRow ||
                bf1.oreField.nodes[s][i].gridCol != bf2.oreField.nodes[s][i].gridCol) {
                anyDifferent = true;
            }
        }
    }
    assert(anyDifferent);
    printf("  PASS: test_different_seed_changes_placement\n");
}

static void test_deplete_reuses_slot(void) {
    Battlefield bf = create_test_bf();
    OreNode *node = &bf.oreField.nodes[SIDE_BOTTOM][3];
    int originalSlot = node->slotIndex;
    int originalId = node->id;

    bool ok = ore_deplete_and_respawn(&bf, originalId);
    assert(ok);

    // Same slot, same id
    node = &bf.oreField.nodes[SIDE_BOTTOM][3];
    assert(node->slotIndex == originalSlot);
    assert(node->id == originalId);
    assert(node->active);
    assert(node->claimedByEntityId == -1);
    printf("  PASS: test_deplete_reuses_slot\n");
}

static void test_deplete_respawn_changes_cell(void) {
    Battlefield bf = create_test_bf();
    OreNode *node = &bf.oreField.nodes[SIDE_BOTTOM][3];
    int oldRow = node->gridRow;
    int oldCol = node->gridCol;

    bool ok = ore_deplete_and_respawn(&bf, node->id);
    assert(ok);

    node = &bf.oreField.nodes[SIDE_BOTTOM][3];
    assert(node->gridRow != oldRow || node->gridCol != oldCol);
    printf("  PASS: test_deplete_respawn_changes_cell\n");
}

static void test_deplete_maintains_count(void) {
    Battlefield bf = create_test_bf();
    int nodeId = bf.oreField.nodes[SIDE_BOTTOM][0].id;
    bool ok = ore_deplete_and_respawn(&bf, nodeId);
    assert(ok);
    assert(ore_active_count(&bf, SIDE_BOTTOM) == ORE_MATCH_COUNT_PER_SIDE);
    printf("  PASS: test_deplete_maintains_count\n");
}

static void test_deplete_respawn_in_same_territory(void) {
    Battlefield bf = create_test_bf();
    int nodeId = bf.oreField.nodes[SIDE_BOTTOM][2].id;
    ore_deplete_and_respawn(&bf, nodeId);
    OreNode *node = &bf.oreField.nodes[SIDE_BOTTOM][2];
    assert(node->worldPos.v.y >= (float)SEAM_Y);
    assert(node->worldPos.v.y <= (float)BOARD_HEIGHT);
    printf("  PASS: test_deplete_respawn_in_same_territory\n");
}

static void test_deplete_respawn_clears_exclusions(void) {
    Battlefield bf = create_test_bf();
    float laneClearPx  = ORE_LANE_CLEARANCE_CELLS  * ORE_GRID_CELL_SIZE_PX;
    float baseClearPx  = ORE_BASE_CLEARANCE_CELLS   * ORE_GRID_CELL_SIZE_PX;
    float spawnClearPx = ORE_SPAWN_CLEARANCE_CELLS  * ORE_GRID_CELL_SIZE_PX;
    float nodeClearPx  = ORE_NODE_CLEARANCE_CELLS   * ORE_GRID_CELL_SIZE_PX;

    int nodeId = bf.oreField.nodes[SIDE_BOTTOM][0].id;
    ore_deplete_and_respawn(&bf, nodeId);
    OreNode *n = &bf.oreField.nodes[SIDE_BOTTOM][0];

    // Lane clearance
    for (int ls = 0; ls < 2; ls++) {
        for (int lane = 0; lane < 3; lane++) {
            for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
                CanonicalPos a = bf_waypoint(&bf, (BattleSide)ls, lane, wp);
                CanonicalPos b = bf_waypoint(&bf, (BattleSide)ls, lane, wp + 1);
                float dist = test_pt_seg_dist(n->worldPos.v, a.v, b.v);
                assert(dist >= laneClearPx - 0.01f);
            }
        }
    }

    // Base clearance
    CanonicalPos base = bf_base_anchor(&bf, SIDE_BOTTOM);
    assert(bf_distance(n->worldPos, base) >= baseClearPx - 0.01f);

    // Spawn clearance
    for (int slot = 0; slot < NUM_CARD_SLOTS; slot++) {
        CanonicalPos anchor = bf_spawn_pos(&bf, SIDE_BOTTOM, slot);
        assert(bf_distance(n->worldPos, anchor) >= spawnClearPx - 0.01f);
    }

    // Inter-node clearance
    for (int j = 1; j < ORE_MATCH_COUNT_PER_SIDE; j++) {
        OreNode *other = &bf.oreField.nodes[SIDE_BOTTOM][j];
        assert(bf_distance(n->worldPos, other->worldPos) >= nodeClearPx - 0.01f);
    }
    printf("  PASS: test_deplete_respawn_clears_exclusions\n");
}

static void test_multiple_depletions_stable(void) {
    Battlefield bf = create_test_bf();
    for (int round = 0; round < 8; round++) {
        int nodeId = bf.oreField.nodes[SIDE_BOTTOM][round % ORE_MATCH_COUNT_PER_SIDE].id;
        bool ok = ore_deplete_and_respawn(&bf, nodeId);
        assert(ok);
        assert(ore_active_count(&bf, SIDE_BOTTOM) == ORE_MATCH_COUNT_PER_SIDE);
    }
    printf("  PASS: test_multiple_depletions_stable\n");
}

static void test_claim_succeeds(void) {
    Battlefield bf = create_test_bf();
    int nodeId = bf.oreField.nodes[SIDE_BOTTOM][0].id;
    bool ok = ore_claim(&bf, nodeId, 42);
    assert(ok);
    OreNode *n = ore_get_node(&bf, nodeId);
    assert(n->claimedByEntityId == 42);
    printf("  PASS: test_claim_succeeds\n");
}

static void test_claim_fails_on_claimed(void) {
    Battlefield bf = create_test_bf();
    int nodeId = bf.oreField.nodes[SIDE_BOTTOM][0].id;
    ore_claim(&bf, nodeId, 42);
    bool ok = ore_claim(&bf, nodeId, 99);
    assert(!ok);
    printf("  PASS: test_claim_fails_on_claimed\n");
}

static void test_claim_fails_on_invalid_id(void) {
    Battlefield bf = create_test_bf();
    bool ok = ore_claim(&bf, -1, 42);
    assert(!ok);
    ok = ore_claim(&bf, 999, 42);
    assert(!ok);
    printf("  PASS: test_claim_fails_on_invalid_id\n");
}

static void test_claim_fails_on_negative_entity(void) {
    Battlefield bf = create_test_bf();
    int nodeId = bf.oreField.nodes[SIDE_BOTTOM][0].id;
    bool ok = ore_claim(&bf, nodeId, -1);
    assert(!ok);

    OreNode *n = ore_get_node(&bf, nodeId);
    assert(n->claimedByEntityId == -1);

    OreNode *found = ore_find_nearest_available(&bf, SIDE_BOTTOM, n->worldPos.v);
    assert(found != NULL && found->id == nodeId);
    printf("  PASS: test_claim_fails_on_negative_entity\n");
}

static void test_release_clears_claim(void) {
    Battlefield bf = create_test_bf();
    int nodeId = bf.oreField.nodes[SIDE_TOP][1].id;
    ore_claim(&bf, nodeId, 42);
    ore_release_claim(&bf, nodeId, 42);
    OreNode *n = ore_get_node(&bf, nodeId);
    assert(n->claimedByEntityId == -1);
    printf("  PASS: test_release_clears_claim\n");
}

static void test_release_wrong_entity_noop(void) {
    Battlefield bf = create_test_bf();
    int nodeId = bf.oreField.nodes[SIDE_TOP][0].id;
    ore_claim(&bf, nodeId, 42);
    ore_release_claim(&bf, nodeId, 99);  // wrong entity
    OreNode *n = ore_get_node(&bf, nodeId);
    assert(n->claimedByEntityId == 42);  // unchanged
    printf("  PASS: test_release_wrong_entity_noop\n");
}

static void test_release_all_for_entity(void) {
    Battlefield bf = create_test_bf();
    // Claim 3 nodes across both sides for entity 42
    ore_claim(&bf, bf.oreField.nodes[SIDE_BOTTOM][0].id, 42);
    ore_claim(&bf, bf.oreField.nodes[SIDE_BOTTOM][3].id, 42);
    ore_claim(&bf, bf.oreField.nodes[SIDE_TOP][1].id, 42);
    // Claim one for entity 99
    ore_claim(&bf, bf.oreField.nodes[SIDE_TOP][2].id, 99);

    ore_release_claims_for_entity(&bf, 42);

    assert(bf.oreField.nodes[SIDE_BOTTOM][0].claimedByEntityId == -1);
    assert(bf.oreField.nodes[SIDE_BOTTOM][3].claimedByEntityId == -1);
    assert(bf.oreField.nodes[SIDE_TOP][1].claimedByEntityId == -1);
    // Entity 99's claim untouched
    assert(bf.oreField.nodes[SIDE_TOP][2].claimedByEntityId == 99);
    printf("  PASS: test_release_all_for_entity\n");
}

static void test_find_nearest_skips_claimed(void) {
    Battlefield bf = create_test_bf();
    // Claim all but one on bottom side
    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE - 1; i++) {
        ore_claim(&bf, bf.oreField.nodes[SIDE_BOTTOM][i].id, 10 + i);
    }
    int lastSlot = ORE_MATCH_COUNT_PER_SIDE - 1;
    OreNode *expected = &bf.oreField.nodes[SIDE_BOTTOM][lastSlot];
    Vector2 from = { 540.0f, 1440.0f };
    OreNode *found = ore_find_nearest_available(&bf, SIDE_BOTTOM, from);
    assert(found == expected);
    printf("  PASS: test_find_nearest_skips_claimed\n");
}

static void test_find_nearest_returns_null_all_claimed(void) {
    Battlefield bf = create_test_bf();
    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
        ore_claim(&bf, bf.oreField.nodes[SIDE_BOTTOM][i].id, 10 + i);
    }
    Vector2 from = { 540.0f, 1440.0f };
    OreNode *found = ore_find_nearest_available(&bf, SIDE_BOTTOM, from);
    assert(found == NULL);
    printf("  PASS: test_find_nearest_returns_null_all_claimed\n");
}

static void test_deplete_invalid_id(void) {
    Battlefield bf = create_test_bf();
    bool ok = ore_deplete_and_respawn(&bf, -1);
    assert(!ok);
    ok = ore_deplete_and_respawn(&bf, 999);
    assert(!ok);
    printf("  PASS: test_deplete_invalid_id\n");
}

static void test_node_ids_stable(void) {
    Battlefield bf = create_test_bf();
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
            OreNode *n = &bf.oreField.nodes[s][i];
            assert(n->id == s * ORE_MATCH_COUNT_PER_SIDE + i);
            assert(n->side == (BattleSide)s);
            assert(n->slotIndex == i);
        }
    }
    printf("  PASS: test_node_ids_stable\n");
}

static void test_get_node_returns_correct(void) {
    Battlefield bf = create_test_bf();
    for (int id = 0; id < ORE_MATCH_COUNT_PER_SIDE * 2; id++) {
        OreNode *n = ore_get_node(&bf, id);
        assert(n != NULL);
        assert(n->id == id);
    }
    assert(ore_get_node(&bf, -1) == NULL);
    assert(ore_get_node(&bf, ORE_MATCH_COUNT_PER_SIDE * 2) == NULL);
    printf("  PASS: test_get_node_returns_correct\n");
}

/* Point-to-segment distance unit tests */
static void test_point_to_segment_perpendicular(void) {
    Vector2 p = {5.0f, 5.0f};
    Vector2 a = {0.0f, 0.0f};
    Vector2 b = {10.0f, 0.0f};
    assert(approx_eq(test_pt_seg_dist(p, a, b), 5.0f, 0.01f));
    printf("  PASS: test_point_to_segment_perpendicular\n");
}

static void test_point_to_segment_closest_to_a(void) {
    Vector2 p = {-3.0f, 4.0f};
    Vector2 a = {0.0f, 0.0f};
    Vector2 b = {10.0f, 0.0f};
    assert(approx_eq(test_pt_seg_dist(p, a, b), 5.0f, 0.01f));
    printf("  PASS: test_point_to_segment_closest_to_a\n");
}

static void test_point_to_segment_closest_to_b(void) {
    Vector2 p = {13.0f, 4.0f};
    Vector2 a = {0.0f, 0.0f};
    Vector2 b = {10.0f, 0.0f};
    assert(approx_eq(test_pt_seg_dist(p, a, b), 5.0f, 0.01f));
    printf("  PASS: test_point_to_segment_closest_to_b\n");
}

static void test_point_to_segment_zero_length(void) {
    Vector2 p = {3.0f, 4.0f};
    Vector2 a = {0.0f, 0.0f};
    Vector2 b = {0.0f, 0.0f};
    assert(approx_eq(test_pt_seg_dist(p, a, b), 5.0f, 0.01f));
    printf("  PASS: test_point_to_segment_zero_length\n");
}

/* ---- main ---- */
int main(void) {
    printf("Running ore tests...\n");

    /* Initialization */
    test_init_bottom_count();
    test_init_top_count();
    test_all_bottom_in_territory();
    test_all_top_in_territory();
    test_ore_obeys_edge_margin();
    test_node_ids_stable();
    test_get_node_returns_correct();

    /* Placement constraints */
    test_ore_clears_lane_geometry();
    test_ore_clears_base_anchor();
    test_ore_clears_spawn_anchors();
    test_ore_no_overlapping_nodes();

    /* RNG determinism */
    test_deterministic_same_seed();
    test_different_seed_changes_placement();

    /* Depletion / respawn */
    test_deplete_reuses_slot();
    test_deplete_respawn_changes_cell();
    test_deplete_maintains_count();
    test_deplete_respawn_in_same_territory();
    test_deplete_respawn_clears_exclusions();
    test_multiple_depletions_stable();
    test_deplete_invalid_id();

    /* Claim lifecycle */
    test_claim_succeeds();
    test_claim_fails_on_claimed();
    test_claim_fails_on_invalid_id();
    test_claim_fails_on_negative_entity();
    test_release_clears_claim();
    test_release_wrong_entity_noop();
    test_release_all_for_entity();
    test_find_nearest_skips_claimed();
    test_find_nearest_returns_null_all_claimed();

    /* Point-to-segment geometry */
    test_point_to_segment_perpendicular();
    test_point_to_segment_closest_to_a();
    test_point_to_segment_closest_to_b();
    test_point_to_segment_zero_length();

    printf("\nAll 29 tests passed!\n");
    return 0;
}
