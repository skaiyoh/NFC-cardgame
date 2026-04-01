/*
 * Unit tests for src/core/battlefield_math.c
 *
 * Self-contained: redefines minimal type stubs and includes battlefield_math.c
 * directly to avoid the heavy include chain (Raylib, sqlite3, biome).
 * Compiles with `make test_battlefield_math` using only -lm.
 *
 * Uses the header-guard-override pattern from test_pathfinding.c.
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

/* ---- Prevent battlefield_math.c's includes from pulling in heavy headers ---- */
#define NFC_CARDGAME_CONFIG_H

/* ---- Config defines (must match src/core/config.h) ---- */
#define BOARD_WIDTH        1080
#define BOARD_HEIGHT       1920
#define SEAM_Y             960
#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.3f
#define PI_F               3.14159265f
#define NUM_CARD_SLOTS     3

/* ---- Minimal type stubs ---- */
#define VECTOR2_DEFINED
typedef struct { float x; float y; } Vector2;

/* ---- Include production code under test ---- */
#include "../src/core/battlefield_math.c"

/* ---- Test helpers ---- */

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

/* ---- Test: bf_slot_to_lane P1 identity ---- */
static void test_slot_to_lane_p1_identity(void) {
    assert(bf_slot_to_lane(SIDE_BOTTOM, 0) == 0);
    assert(bf_slot_to_lane(SIDE_BOTTOM, 1) == 1);
    assert(bf_slot_to_lane(SIDE_BOTTOM, 2) == 2);
}

/* ---- Test: bf_slot_to_lane P2 mirror ---- */
static void test_slot_to_lane_p2_mirror(void) {
    assert(bf_slot_to_lane(SIDE_TOP, 0) == 2);
    assert(bf_slot_to_lane(SIDE_TOP, 1) == 1);
    assert(bf_slot_to_lane(SIDE_TOP, 2) == 0);
}

/* ---- Test: bf_to_canonical for P1 (SIDE_BOTTOM) identity ---- */
static void test_to_canonical_p1(void) {
    SideLocalPos local = { .v = { 540.0f, 1728.0f }, .side = SIDE_BOTTOM };
    CanonicalPos result = bf_to_canonical(local, 1080.0f);
    assert(approx_eq(result.v.x, 540.0f, 0.01f));
    assert(approx_eq(result.v.y, 1728.0f, 0.01f));
}

/* ---- Test: bf_to_canonical for P2 (SIDE_TOP) lateral mirror ---- */
static void test_to_canonical_p2(void) {
    /* u = 200/1080; canonical x = (1 - u) * 1080 = 1080 - 200 = 880 */
    SideLocalPos local = { .v = { 200.0f, 400.0f }, .side = SIDE_TOP };
    CanonicalPos result = bf_to_canonical(local, 1080.0f);
    assert(approx_eq(result.v.x, 880.0f, 0.01f));
    assert(approx_eq(result.v.y, 400.0f, 0.01f));

    /* Edge case: x=0 maps to 1080 */
    SideLocalPos edge = { .v = { 0.0f, 500.0f }, .side = SIDE_TOP };
    CanonicalPos edgeResult = bf_to_canonical(edge, 1080.0f);
    assert(approx_eq(edgeResult.v.x, 1080.0f, 0.01f));

    /* Edge case: x=1080 maps to 0 */
    SideLocalPos edge2 = { .v = { 1080.0f, 500.0f }, .side = SIDE_TOP };
    CanonicalPos edge2Result = bf_to_canonical(edge2, 1080.0f);
    assert(approx_eq(edge2Result.v.x, 0.0f, 0.01f));
}

/* ---- Test: roundtrip bf_to_canonical <-> bf_to_local ---- */
static void test_to_local_roundtrip(void) {
    /* Test SIDE_BOTTOM roundtrip */
    SideLocalPos localBottom = { .v = { 300.0f, 1200.0f }, .side = SIDE_BOTTOM };
    CanonicalPos canonical = bf_to_canonical(localBottom, 1080.0f);
    SideLocalPos back = bf_to_local(canonical, SIDE_BOTTOM, 1080.0f);
    assert(approx_eq(back.v.x, localBottom.v.x, 0.01f));
    assert(approx_eq(back.v.y, localBottom.v.y, 0.01f));
    assert(back.side == SIDE_BOTTOM);

    /* Test SIDE_TOP roundtrip */
    SideLocalPos localTop = { .v = { 700.0f, 300.0f }, .side = SIDE_TOP };
    CanonicalPos canonical2 = bf_to_canonical(localTop, 1080.0f);
    SideLocalPos back2 = bf_to_local(canonical2, SIDE_TOP, 1080.0f);
    assert(approx_eq(back2.v.x, localTop.v.x, 0.01f));
    assert(approx_eq(back2.v.y, localTop.v.y, 0.01f));
    assert(back2.side == SIDE_TOP);
}

/* ---- Test: bf_distance simple Euclidean ---- */
static void test_distance_simple(void) {
    CanonicalPos a = { .v = { 0.0f, 0.0f } };
    CanonicalPos b = { .v = { 3.0f, 4.0f } };
    float d = bf_distance(a, b);
    assert(approx_eq(d, 5.0f, 0.01f));
}

/* ---- Test: bf_distance same point ---- */
static void test_distance_same_point(void) {
    CanonicalPos p = { .v = { 42.0f, 99.0f } };
    float d = bf_distance(p, p);
    assert(approx_eq(d, 0.0f, 0.01f));
}

/* ---- Test: bf_in_bounds valid positions ---- */
static void test_in_bounds_valid(void) {
    /* Interior point */
    CanonicalPos mid = { .v = { 540.0f, 960.0f } };
    assert(bf_in_bounds(mid, 1080.0f, 1920.0f) == true);

    /* Corner: origin */
    CanonicalPos origin = { .v = { 0.0f, 0.0f } };
    assert(bf_in_bounds(origin, 1080.0f, 1920.0f) == true);

    /* Corner: max */
    CanonicalPos maxPos = { .v = { 1080.0f, 1920.0f } };
    assert(bf_in_bounds(maxPos, 1080.0f, 1920.0f) == true);
}

/* ---- Test: bf_in_bounds invalid positions ---- */
static void test_in_bounds_invalid(void) {
    /* x < 0 */
    CanonicalPos negX = { .v = { -1.0f, 500.0f } };
    assert(bf_in_bounds(negX, 1080.0f, 1920.0f) == false);

    /* x > boardWidth */
    CanonicalPos overX = { .v = { 1081.0f, 500.0f } };
    assert(bf_in_bounds(overX, 1080.0f, 1920.0f) == false);

    /* y < 0 */
    CanonicalPos negY = { .v = { 500.0f, -1.0f } };
    assert(bf_in_bounds(negY, 1080.0f, 1920.0f) == false);

    /* y > boardHeight */
    CanonicalPos overY = { .v = { 500.0f, 1921.0f } };
    assert(bf_in_bounds(overY, 1080.0f, 1920.0f) == false);
}

/* ---- Test: bf_crosses_seam ---- */
static void test_crosses_seam(void) {
    /* Sprite at y=970, height=80: top=930, bottom=1010 => crosses 960 */
    CanonicalPos crossing = { .v = { 540.0f, 970.0f } };
    assert(bf_crosses_seam(crossing, 80.0f, 960.0f) == true);

    /* Sprite at y=950, height=80: top=910, bottom=990 => crosses 960 */
    CanonicalPos crossing2 = { .v = { 540.0f, 950.0f } };
    assert(bf_crosses_seam(crossing2, 80.0f, 960.0f) == true);

    /* Sprite at y=500, height=80: top=460, bottom=540 => does not cross 960 */
    CanonicalPos noCross = { .v = { 540.0f, 500.0f } };
    assert(bf_crosses_seam(noCross, 80.0f, 960.0f) == false);

    /* Sprite at y=1500, height=80: top=1460, bottom=1540 => does not cross 960 */
    CanonicalPos farBelow = { .v = { 540.0f, 1500.0f } };
    assert(bf_crosses_seam(farBelow, 80.0f, 960.0f) == false);

    /* Sprite exactly at seam: y=960, height=80: top=920, bottom=1000 => crosses */
    CanonicalPos onSeam = { .v = { 540.0f, 960.0f } };
    assert(bf_crosses_seam(onSeam, 80.0f, 960.0f) == true);
}

/* ---- Test: bf_side_for_pos ---- */
static void test_side_for_pos(void) {
    /* y=500 is above seam -> SIDE_TOP */
    CanonicalPos top = { .v = { 540.0f, 500.0f } };
    assert(bf_side_for_pos(top, 960.0f) == SIDE_TOP);

    /* y=1500 is below seam -> SIDE_BOTTOM */
    CanonicalPos bottom = { .v = { 540.0f, 1500.0f } };
    assert(bf_side_for_pos(bottom, 960.0f) == SIDE_BOTTOM);

    /* y=960 is exactly at seam -> SIDE_BOTTOM (on seam = bottom territory) */
    CanonicalPos seam = { .v = { 540.0f, 960.0f } };
    assert(bf_side_for_pos(seam, 960.0f) == SIDE_BOTTOM);
}

/* ---- Test: BF_ASSERT_IN_BOUNDS for valid pos does not fire ---- */
static void test_bf_assert_in_bounds(void) {
    CanonicalPos valid = { .v = { 540.0f, 960.0f } };
    /* This should not abort */
    BF_ASSERT_IN_BOUNDS(valid, 1080.0f, 1920.0f);

    /* Boundary positions */
    CanonicalPos corner = { .v = { 0.0f, 0.0f } };
    BF_ASSERT_IN_BOUNDS(corner, 1080.0f, 1920.0f);

    CanonicalPos maxCorner = { .v = { 1080.0f, 1920.0f } };
    BF_ASSERT_IN_BOUNDS(maxCorner, 1080.0f, 1920.0f);
}

/* ---- main ---- */
int main(void) {
    printf("Running battlefield_math tests...\n");
    test_slot_to_lane_p1_identity();  printf("  PASS: test_slot_to_lane_p1_identity\n");
    test_slot_to_lane_p2_mirror();    printf("  PASS: test_slot_to_lane_p2_mirror\n");
    test_to_canonical_p1();           printf("  PASS: test_to_canonical_p1\n");
    test_to_canonical_p2();           printf("  PASS: test_to_canonical_p2\n");
    test_to_local_roundtrip();        printf("  PASS: test_to_local_roundtrip\n");
    test_distance_simple();           printf("  PASS: test_distance_simple\n");
    test_distance_same_point();       printf("  PASS: test_distance_same_point\n");
    test_in_bounds_valid();           printf("  PASS: test_in_bounds_valid\n");
    test_in_bounds_invalid();         printf("  PASS: test_in_bounds_invalid\n");
    test_crosses_seam();              printf("  PASS: test_crosses_seam\n");
    test_side_for_pos();              printf("  PASS: test_side_for_pos\n");
    test_bf_assert_in_bounds();       printf("  PASS: test_bf_assert_in_bounds\n");
    printf("\nAll 12 tests passed!\n");
    return 0;
}
