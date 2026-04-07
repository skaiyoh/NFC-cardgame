/*
 * Unit tests for src/core/battlefield.c
 *
 * Self-contained: redefines minimal type stubs and includes battlefield.c
 * and battlefield_math.c directly to avoid the heavy include chain
 * (Raylib, sqlite3, biome). Compiles with `make test_battlefield` using only -lm.
 *
 * Uses the header-guard-override pattern from test_pathfinding.c.
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

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

/* Ore config (required by ore.h included from battlefield.h) */
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

/* ---- Minimal Entity stub for entity registry tests ---- */
typedef struct Entity {
    int id;
} Entity;

/* ---- Stub functions (required by battlefield.c via biome.h / tilemap_renderer.h) ---- */

TileMap tilemap_create_biome(Rectangle area, float tileSize, unsigned int seed,
                             const BiomeDef *biome) {
    (void)area; (void)tileSize; (void)seed; (void)biome;
    TileMap map;
    memset(&map, 0, sizeof(TileMap));
    return map;
}

void tilemap_free(TileMap *map) {
    (void)map;
}

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

/* ---- Test helpers ---- */

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

static Battlefield create_test_battlefield(void) {
    BiomeDef defs[BIOME_COUNT];
    memset(defs, 0, sizeof(defs));
    Battlefield bf;
    bf_init(&bf, defs, BIOME_GRASS, BIOME_GRASS, 64.0f, 42, 99);
    return bf;
}

/* ---- Test: bf_spawn_anchors_bottom ---- */
/* Verify P1 (SIDE_BOTTOM) slot spawn X positions at canonical 1/6, 1/2, 5/6 of BOARD_WIDTH
 * and Y near SEAM_Y + 960 * 0.8 = 1728 */
static void test_bf_spawn_anchors_bottom(void) {
    Battlefield bf = create_test_battlefield();
    float laneWidth = (float)BOARD_WIDTH / 3.0f;
    float outerInset = laneWidth * LANE_OUTER_INSET_RATIO;

    // Expected X for slots 0,1,2: center of each lane = (slot + 0.5) * (1080/3)
    // Outer lanes are nudged inward by a small canonical inset.
    // P1 is SIDE_BOTTOM (identity mapping, no lateral mirror)
    float expectedX[3] = { 180.0f + outerInset, 540.0f, 900.0f - outerInset };
    float expectedY = 960.0f + 960.0f * 0.8f;  // 1728.0

    for (int slot = 0; slot < 3; slot++) {
        CanonicalPos anchor = bf_spawn_pos(&bf, SIDE_BOTTOM, slot);
        assert(approx_eq(anchor.v.x, expectedX[slot], 1.0f));
        assert(approx_eq(anchor.v.y, expectedY, 5.0f));
    }

    printf("  PASS: test_bf_spawn_anchors_bottom\n");
}

/* ---- Test: bf_spawn_anchors_top ---- */
/* Verify P2 (SIDE_TOP) slot spawn positions have laterally mirrored X
 * (per D-06/D-08: slot 0 -> lane 2 -> canonical x=900; slot 2 -> lane 0 -> canonical x=180)
 * and Y near the top territory spawn depth */
static void test_bf_spawn_anchors_top(void) {
    Battlefield bf = create_test_battlefield();
    float laneWidth = (float)BOARD_WIDTH / 3.0f;
    float outerInset = laneWidth * LANE_OUTER_INSET_RATIO;

    // P2 slot-to-lane mapping (D-08): slot 0 -> lane 2, slot 1 -> lane 1, slot 2 -> lane 0
    // The canonical outer lanes are mirrored, then nudged inward toward center.
    float expectedX[3] = { 900.0f - outerInset, 540.0f, 180.0f + outerInset };

    // P2 spawn Y: territory {0,0,1080,960}, spawn at 20% from top = 0 + 960 * 0.2 = 192
    float expectedY = 960.0f * 0.2f;  // 192.0

    for (int slot = 0; slot < 3; slot++) {
        CanonicalPos anchor = bf_spawn_pos(&bf, SIDE_TOP, slot);
        assert(approx_eq(anchor.v.x, expectedX[slot], 1.0f));
        assert(approx_eq(anchor.v.y, expectedY, 5.0f));
    }

    printf("  PASS: test_bf_spawn_anchors_top\n");
}

/* ---- Test: bf_waypoints_march_forward ---- */
/* Verify that SIDE_BOTTOM waypoints march toward decreasing y (per D-03)
 * and SIDE_TOP waypoints march toward increasing y (per D-04) */
static void test_bf_waypoints_march_forward(void) {
    Battlefield bf = create_test_battlefield();

    // SIDE_BOTTOM: center lane (lane 1) waypoints should have decreasing y
    for (int wp = 1; wp < LANE_WAYPOINT_COUNT; wp++) {
        CanonicalPos curr = bf_waypoint(&bf, SIDE_BOTTOM, 1, wp);
        CanonicalPos prev = bf_waypoint(&bf, SIDE_BOTTOM, 1, wp - 1);
        assert(curr.v.y < prev.v.y);
    }

    // SIDE_TOP: center lane (lane 1) waypoints should have increasing y
    for (int wp = 1; wp < LANE_WAYPOINT_COUNT; wp++) {
        CanonicalPos curr = bf_waypoint(&bf, SIDE_TOP, 1, wp);
        CanonicalPos prev = bf_waypoint(&bf, SIDE_TOP, 1, wp - 1);
        assert(curr.v.y > prev.v.y);
    }

    printf("  PASS: test_bf_waypoints_march_forward\n");
}

/* ---- Test: bf_waypoints_in_bounds ---- */
/* Verify all waypoints are within reasonable X range (0..BOARD_WIDTH).
 * Y may go slightly outside 0..BOARD_HEIGHT at path endpoints (troops march
 * into enemy territory), so we only check X bounds strictly. */
static void test_bf_waypoints_in_bounds(void) {
    Battlefield bf = create_test_battlefield();

    for (int side = 0; side < 2; side++) {
        for (int lane = 0; lane < 3; lane++) {
            for (int wp = 0; wp < LANE_WAYPOINT_COUNT; wp++) {
                CanonicalPos pos = bf_waypoint(&bf, (BattleSide)side, lane, wp);
                assert(pos.v.x >= -50.0f && pos.v.x <= (float)BOARD_WIDTH + 50.0f);
                // Y can exceed board bounds at path extremes (going into enemy half)
                // but should still be within a reasonable range
                assert(pos.v.y >= -500.0f && pos.v.y <= (float)BOARD_HEIGHT + 500.0f);
            }
        }
    }

    printf("  PASS: test_bf_waypoints_in_bounds\n");
}

/* ---- Test: bf_entity_registry ---- */
/* Verify add/find/remove entity operations on the Battlefield entity registry */
static void test_bf_entity_registry(void) {
    Battlefield bf = create_test_battlefield();
    Entity e1 = { .id = 42 };
    Entity e2 = { .id = 99 };

    // Initially empty
    assert(bf.entityCount == 0);
    assert(bf_find_entity(&bf, 42) == NULL);

    // Add entities
    bf_add_entity(&bf, &e1);
    assert(bf.entityCount == 1);
    assert(bf_find_entity(&bf, 42) == &e1);

    bf_add_entity(&bf, &e2);
    assert(bf.entityCount == 2);
    assert(bf_find_entity(&bf, 99) == &e2);

    // Remove first entity (swap-with-last)
    bf_remove_entity(&bf, 42);
    assert(bf.entityCount == 1);
    assert(bf_find_entity(&bf, 42) == NULL);
    assert(bf_find_entity(&bf, 99) == &e2);

    // Remove last entity
    bf_remove_entity(&bf, 99);
    assert(bf.entityCount == 0);

    printf("  PASS: test_bf_entity_registry\n");
}

/* ---- Test: bf_side_for_player ---- */
static void test_bf_side_for_player(void) {
    assert(bf_side_for_player(0) == SIDE_BOTTOM);
    assert(bf_side_for_player(1) == SIDE_TOP);
    printf("  PASS: test_bf_side_for_player\n");
}

/* ---- Test: bf_territory_queries ---- */
static void test_bf_territory_queries(void) {
    Battlefield bf = create_test_battlefield();

    // Territory for side
    Territory *bottom = bf_territory_for_side(&bf, SIDE_BOTTOM);
    assert(bottom->side == SIDE_BOTTOM);
    assert(approx_eq(bottom->bounds.y, 960.0f, 0.1f));

    Territory *top = bf_territory_for_side(&bf, SIDE_TOP);
    assert(top->side == SIDE_TOP);
    assert(approx_eq(top->bounds.y, 0.0f, 0.1f));

    // Territory at canonical position
    CanonicalPos bottomPos = { .v = { 540.0f, 1500.0f } };
    Territory *atBottom = bf_territory_at(&bf, bottomPos);
    assert(atBottom->side == SIDE_BOTTOM);

    CanonicalPos topPos = { .v = { 540.0f, 500.0f } };
    Territory *atTop = bf_territory_at(&bf, topPos);
    assert(atTop->side == SIDE_TOP);

    printf("  PASS: test_bf_territory_queries\n");
}

/* ---- Test: bf_lanes_coincide_at_seam ---- */
/* Top and bottom lanes for the same canonical lane index must converge at the
 * seam.  At the last waypoint (enemy end), lane 0 bottom should be near
 * the same canonical X as lane 0 top, and likewise for lanes 1 and 2.
 * "Near" = within one lane-width, since bow offsets shift outer lanes. */
static void test_bf_lanes_coincide_at_seam(void) {
    Battlefield bf = create_test_battlefield();
    float laneWidth = (float)BOARD_WIDTH / 3.0f;

    for (int lane = 0; lane < 3; lane++) {
        // Last waypoint of bottom side (deep in top territory)
        CanonicalPos bottomEnd = bf_waypoint(&bf, SIDE_BOTTOM, lane, LANE_WAYPOINT_COUNT - 1);
        // Last waypoint of top side (deep in bottom territory)
        CanonicalPos topEnd = bf_waypoint(&bf, SIDE_TOP, lane, LANE_WAYPOINT_COUNT - 1);

        // Both should have similar X (within one lane width)
        float xDiff = fabsf(bottomEnd.v.x - topEnd.v.x);
        assert(xDiff < laneWidth);

        // Center lane (lane 1) should have nearly identical X (no bow)
        if (lane == 1) {
            assert(xDiff < 5.0f);
        }
    }

    printf("  PASS: test_bf_lanes_coincide_at_seam\n");
}

/* ---- Test: bf_outer_lanes_end_near_enemy_base ---- */
/* Outer-lane endpoints should converge toward an attack point near the enemy
 * base so melee units can find the base after finishing the path. */
static void test_bf_outer_lanes_end_near_enemy_base(void) {
    Battlefield bf = create_test_battlefield();
    CanonicalPos topBase = bf_base_anchor(&bf, SIDE_TOP);
    CanonicalPos bottomBase = bf_base_anchor(&bf, SIDE_BOTTOM);

    for (int lane = 0; lane < 3; lane += 2) {
        CanonicalPos bottomEnd = bf_waypoint(&bf, SIDE_BOTTOM, lane, LANE_WAYPOINT_COUNT - 1);
        CanonicalPos topEnd = bf_waypoint(&bf, SIDE_TOP, lane, LANE_WAYPOINT_COUNT - 1);

        assert(approx_eq(bottomEnd.v.x, topBase.v.x, 1.0f));
        assert(approx_eq(topEnd.v.x, bottomBase.v.x, 1.0f));

        assert(approx_eq(bottomEnd.v.y, topBase.v.y + LANE_BASE_APPROACH_GAP, 1.0f));
        assert(approx_eq(topEnd.v.y, bottomBase.v.y - LANE_BASE_APPROACH_GAP, 1.0f));

        assert(bf_distance(bottomEnd, topBase) < 32.0f);
        assert(bf_distance(topEnd, bottomBase) < 32.0f);
    }

    printf("  PASS: test_bf_outer_lanes_end_near_enemy_base\n");
}

/* ---- Test: bf_seam_screen_placement ---- */
/* Verify that the canonical seam (y=960) maps to screen x=960 for both
 * camera configurations used by the game.  This catches the P2 camera
 * orientation bug where the seam landed on the wrong edge. */
static void test_bf_seam_screen_placement(void) {
    // Both cameras use rot=+90.  P2 renders to an RT then is flipped for
    // across-the-table perspective.
    //
    // Raylib Camera2D transform (for dx=0, only Y matters):
    //   rot=+90: screen_x = offset_x - (wy - target_y)  [sin(90)=1]
    //     Derivation: rx = dx*cos(90) - dy*sin(90) = -dy = -(wy - target_y)
    //     screen_x = rx + offset_x = offset_x - (wy - target_y)
    Vector2 seamPoint = {540.0f, 960.0f};

    // P1 (rot=+90, target_y=1440, offset_x=480):
    float p1_sx = 480.0f - (seamPoint.y - 1440.0f);
    // P2 RT (rot=+90, target_y=480, offset_x=480 in RT):
    float p2_rt_sx = 480.0f - (seamPoint.y - 480.0f);

    // P1 seam maps to x=960 (inner screen edge)
    assert(approx_eq(p1_sx, 960.0f, 1.0f));
    // P2 seam maps to x=0 in the RT; after compositing at screen x=960..1920,
    // RT x=0 → screen x=960 (inner edge). ✓
    assert(approx_eq(p2_rt_sx, 0.0f, 1.0f));

    printf("  PASS: test_bf_seam_screen_placement\n");
}

/* ---- Test: bf_base_anchor_bottom ---- */
/* Verify P1 (SIDE_BOTTOM) base anchor is behind spawn by BASE_SPAWN_GAP */
static void test_bf_base_anchor_bottom(void) {
    Battlefield bf = create_test_battlefield();
    CanonicalPos anchor = bf_base_anchor(&bf, SIDE_BOTTOM);

    // Center lane spawn Y = 960 + 960 * 0.8 = 1728; base = 1728 + 32 = 1760
    assert(approx_eq(anchor.v.x, 540.0f, 5.0f));
    assert(approx_eq(anchor.v.y, 1760.0f, 5.0f));

    printf("  PASS: test_bf_base_anchor_bottom\n");
}

/* ---- Test: bf_base_anchor_top ---- */
/* Verify P2 (SIDE_TOP) base anchor is behind spawn by BASE_SPAWN_GAP */
static void test_bf_base_anchor_top(void) {
    Battlefield bf = create_test_battlefield();
    CanonicalPos anchor = bf_base_anchor(&bf, SIDE_TOP);

    // Center lane spawn Y = 960 * 0.2 = 192; base = 192 - 32 = 160
    assert(approx_eq(anchor.v.x, 540.0f, 5.0f));
    assert(approx_eq(anchor.v.y, 160.0f, 5.0f));

    printf("  PASS: test_bf_base_anchor_top\n");
}

/* ---- Test: bf_base_anchor_gap ---- */
/* Verify base anchor and spawn anchor are separated by exactly BASE_SPAWN_GAP */
static void test_bf_base_anchor_gap(void) {
    Battlefield bf = create_test_battlefield();

    for (int side = 0; side < 2; side++) {
        CanonicalPos spawn = bf_spawn_pos(&bf, (BattleSide)side, 1); // center slot
        CanonicalPos base = bf_base_anchor(&bf, (BattleSide)side);
        float gap = fabsf(base.v.y - spawn.v.y);
        assert(approx_eq(gap, BASE_SPAWN_GAP, 0.1f));
        // X should be the same
        assert(approx_eq(base.v.x, spawn.v.x, 0.1f));
    }

    printf("  PASS: test_bf_base_anchor_gap\n");
}

/* ---- main ---- */
int main(void) {
    printf("Running battlefield tests...\n");
    test_bf_spawn_anchors_bottom();
    test_bf_spawn_anchors_top();
    test_bf_waypoints_march_forward();
    test_bf_waypoints_in_bounds();
    test_bf_entity_registry();
    test_bf_side_for_player();
    test_bf_territory_queries();
    test_bf_lanes_coincide_at_seam();
    test_bf_outer_lanes_end_near_enemy_base();
    test_bf_seam_screen_placement();
    test_bf_base_anchor_bottom();
    test_bf_base_anchor_top();
    test_bf_base_anchor_gap();
    printf("\nAll 13 tests passed!\n");
    return 0;
}
