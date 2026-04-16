/*
 * Unit tests for src/systems/spawn_placement.c
 *
 * Self-contained: redefines minimal type stubs and includes
 * spawn_placement.c directly to avoid the heavy types.h include chain.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/* ---- Prevent heavy-header pull-in ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_BATTLEFIELD_MATH_H
#define NFC_CARDGAME_SUSTENANCE_H

/* ---- Config defines (mirror src/core/config.h) ---- */
#define NUM_CARD_SLOTS          3
#define MAX_ENTITIES            64
#define BOARD_WIDTH             1080
#define BOARD_HEIGHT            1920
#define PATHFIND_CONTACT_GAP    2.0f

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;

typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;
typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef struct { Vector2 v; } CanonicalPos;

typedef struct Entity {
    int id;
    EntityType type;
    Vector2 position;
    float bodyRadius;
    bool alive;
    bool markedForRemoval;
} Entity;

typedef struct Battlefield {
    float boardWidth;
    float boardHeight;
    CanonicalPos slotSpawnAnchors[2][NUM_CARD_SLOTS];
    Entity *entities[MAX_ENTITIES * 2];
    int entityCount;
} Battlefield;

typedef struct GameState {
    Battlefield battlefield;
} GameState;

/* ---- Stub functions used by spawn_placement.c ---- */
CanonicalPos bf_spawn_pos(const Battlefield *bf, BattleSide side, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) {
        CanonicalPos fallback = { .v = { bf->boardWidth / 2.0f, bf->boardHeight / 2.0f } };
        return fallback;
    }
    return bf->slotSpawnAnchors[side][slotIndex];
}

/* ---- Include production code under test ---- */
#include "../src/systems/spawn_placement.c"

/* ---- Test helpers ---- */
static Battlefield make_bf(void) {
    Battlefield bf;
    memset(&bf, 0, sizeof(bf));
    bf.boardWidth = BOARD_WIDTH;
    bf.boardHeight = BOARD_HEIGHT;
    /* P1 bottom slot 0 at center lane spawn */
    bf.slotSpawnAnchors[SIDE_BOTTOM][0].v = (Vector2){ 540.0f, 1700.0f };
    bf.slotSpawnAnchors[SIDE_BOTTOM][1].v = (Vector2){ 180.0f, 1700.0f };
    bf.slotSpawnAnchors[SIDE_BOTTOM][2].v = (Vector2){ 900.0f, 1700.0f };
    bf.slotSpawnAnchors[SIDE_TOP][0].v    = (Vector2){ 540.0f, 200.0f };
    bf.slotSpawnAnchors[SIDE_TOP][1].v    = (Vector2){ 180.0f, 200.0f };
    bf.slotSpawnAnchors[SIDE_TOP][2].v    = (Vector2){ 900.0f, 200.0f };
    return bf;
}

static Entity make_blocker(Vector2 pos, float radius) {
    static int nextId = 1000;
    Entity e;
    memset(&e, 0, sizeof(e));
    e.id = nextId++;
    e.type = ENTITY_TROOP;
    e.position = pos;
    e.bodyRadius = radius;
    e.alive = true;
    return e;
}

static void bf_add(Battlefield *bf, Entity *e) {
    bf->entities[bf->entityCount++] = e;
}

static void test_empty_battlefield_returns_anchor_center(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.battlefield = make_bf();

    Vector2 out = {0};
    bool found = spawn_find_free_anchor(&gs, SIDE_BOTTOM, 0, 14.0f, &out);

    assert(found == true);
    /* First candidate is the anchor itself (offset 0, row 0). */
    assert(fabsf(out.x - 540.0f) < 0.001f);
    assert(fabsf(out.y - 1700.0f) < 0.001f);
}

static void test_blocker_on_anchor_finds_lateral_offset(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.battlefield = make_bf();

    /* Blocker sitting exactly on the slot 0 anchor. */
    Entity blocker = make_blocker((Vector2){ 540.0f, 1700.0f }, 14.0f);
    bf_add(&gs.battlefield, &blocker);

    Vector2 out = {0};
    bool found = spawn_find_free_anchor(&gs, SIDE_BOTTOM, 0, 14.0f, &out);

    assert(found == true);
    /* Should land at the first lateral offset (-1.25 * 14 = -17.5) away from anchor x. */
    float dx = out.x - 540.0f;
    float dy = out.y - 1700.0f;
    float dist = sqrtf(dx * dx + dy * dy);
    /* Must be non-overlapping with the blocker */
    float minDist = 14.0f + 14.0f + PATHFIND_CONTACT_GAP;
    assert(dist >= minDist);
}

static void test_saturated_slot_returns_false(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.battlefield = make_bf();

    /* Blanket every exact search candidate with a large blocker so the
     * placement search cannot find any free point. */
    static const float lateralMults[] = {
        0.0f, -1.25f, 1.25f, -2.5f, 2.5f, -3.75f, 3.75f, -5.0f, 5.0f
    };
    Entity blockers[36];
    int bi = 0;
    for (int row = 0; row < 4; row++) {
        float y = 1700.0f + 14.0f * (float)row;
        for (int i = 0; i < 9; i++) {
            Vector2 pos = { 540.0f + lateralMults[i] * 14.0f, y };
            blockers[bi] = make_blocker(pos, 30.0f);
            bf_add(&gs.battlefield, &blockers[bi]);
            bi++;
        }
    }

    Vector2 out = {0};
    bool found = spawn_find_free_anchor(&gs, SIDE_BOTTOM, 0, 14.0f, &out);

    assert(found == false);
}

static void test_first_two_rows_blocked_finds_deeper_row(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.battlefield = make_bf();

    static const float lateralMults[] = {
        0.0f, -1.25f, 1.25f, -2.5f, 2.5f, -3.75f, 3.75f, -5.0f, 5.0f
    };
    Entity blockers[9];
    for (int i = 0; i < 9; i++) {
        blockers[i] = make_blocker((Vector2){ 540.0f + lateralMults[i] * 14.0f, 1695.0f }, 10.0f);
        bf_add(&gs.battlefield, &blockers[i]);
    }

    Vector2 out = {0};
    bool found = spawn_find_free_anchor(&gs, SIDE_BOTTOM, 0, 14.0f, &out);

    assert(found == true);
    assert(fabsf(out.x - 540.0f) < 0.001f);
    assert(fabsf(out.y - (1700.0f + 28.0f)) < 0.001f);
}

static void test_dead_blockers_are_ignored(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.battlefield = make_bf();

    /* Dead blocker on the anchor should NOT prevent placement. */
    Entity dead = make_blocker((Vector2){ 540.0f, 1700.0f }, 14.0f);
    dead.alive = false;
    bf_add(&gs.battlefield, &dead);

    Vector2 out = {0};
    bool found = spawn_find_free_anchor(&gs, SIDE_BOTTOM, 0, 14.0f, &out);

    assert(found == true);
    assert(fabsf(out.x - 540.0f) < 0.001f);
    assert(fabsf(out.y - 1700.0f) < 0.001f);
}

static void test_base_near_anchor_keeps_center_spawn_legal(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.battlefield = make_bf();

    /* Match the live game: the home base sits 48 px inward from the raw
     * center-slot spawn depth, which leaves the exact anchor just legal. */
    Entity base = make_blocker((Vector2){ 540.0f, 1652.0f }, 16.0f);
    base.type = ENTITY_BUILDING;
    bf_add(&gs.battlefield, &base);

    Vector2 out = {0};
    bool found = spawn_find_free_anchor(&gs, SIDE_BOTTOM, 0, 14.0f, &out);

    assert(found == true);
    assert(fabsf(out.x - 540.0f) < 0.001f);
    assert(fabsf(out.y - 1700.0f) < 0.001f);

    float dx = out.x - base.position.x;
    float dy = out.y - base.position.y;
    float dist = sqrtf(dx * dx + dy * dy);
    float minDist = 14.0f + 16.0f + PATHFIND_CONTACT_GAP;
    assert(dist >= minDist);
}

static void test_null_safety(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.battlefield = make_bf();

    Vector2 out = {0};
    assert(spawn_find_free_anchor(NULL, SIDE_BOTTOM, 0, 14.0f, &out) == false);
    assert(spawn_find_free_anchor(&gs, SIDE_BOTTOM, 0, 14.0f, NULL) == false);
    assert(spawn_find_free_anchor(&gs, SIDE_BOTTOM, 0, -1.0f, &out) == false);
}

int main(void) {
    printf("Running spawn_placement tests...\n");
    test_empty_battlefield_returns_anchor_center();
    printf("  PASS: test_empty_battlefield_returns_anchor_center\n");
    test_blocker_on_anchor_finds_lateral_offset();
    printf("  PASS: test_blocker_on_anchor_finds_lateral_offset\n");
    test_saturated_slot_returns_false();
    printf("  PASS: test_saturated_slot_returns_false\n");
    test_first_two_rows_blocked_finds_deeper_row();
    printf("  PASS: test_first_two_rows_blocked_finds_deeper_row\n");
    test_dead_blockers_are_ignored();
    printf("  PASS: test_dead_blockers_are_ignored\n");
    test_base_near_anchor_keeps_center_spawn_legal();
    printf("  PASS: test_base_near_anchor_keeps_center_spawn_legal\n");
    test_null_safety();
    printf("  PASS: test_null_safety\n");
    printf("\nAll 7 tests passed!\n");
    return 0;
}
